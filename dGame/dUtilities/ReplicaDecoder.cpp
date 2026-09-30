#include "ReplicaDecoder.h"

#include <algorithm>
#include <array>
#include <functional>
#include <set>

#include "BitStream.h"
#include "CDClientDatabase.h"
#include "eReplicaComponentType.h"
#include "GeneralUtils.h"
#include "magic_enum.hpp"
#include "MessageIdentifiers.h"
#include "ZCompression.h"

namespace {
	using json = nlohmann::json;
	using enum eReplicaComponentType;

	bool IsUtf8(std::string_view text) {
		for (size_t i = 0; i < text.size();) {
			const auto byte = static_cast<uint8_t>(text[i]);
			const size_t length = byte < 0x80 ? 1 : (byte >> 5) == 0x6 ? 2 : (byte >> 4) == 0xE ? 3 : (byte >> 3) == 0x1E ? 4 : 0;
			if (length == 0 || i + length > text.size()) return false;
			for (size_t k = 1; k < length; k++) {
				if ((static_cast<uint8_t>(text[i + k]) & 0xC0) != 0x80) return false;
			}
			i += length;
		}
		return true;
	}

	// Reads values in order; the first read past the end marks the reader as failed and every later read gives 0
	struct Reader {
		RakNet::BitStream& stream;
		bool ok{ true };

		template<typename T> T Get() {
			T value{};
			if (ok && !stream.Read(value)) ok = false;
			return value;
		}
		bool Bit() { return Get<bool>(); }
		std::string Id() { return std::to_string(Get<int64_t>()); }
		json Point() {
			const auto x = Get<float>(), y = Get<float>(), z = Get<float>();
			return json::array({ x, y, z });
		}
		// As the server writes them: x, y, z, w
		json Rotation() {
			const auto x = Get<float>(), y = Get<float>(), z = Get<float>(), w = Get<float>();
			return json::array({ x, y, z, w });
		}
		// A glm quaternion written whole: w, x, y, z
		json RawRotation() {
			const auto w = Get<float>(), x = Get<float>(), y = Get<float>(), z = Get<float>();
			return json::array({ x, y, z, w });
		}
		template<typename Length> std::string WideText() {
			const auto length = Get<Length>();
			std::u16string text;
			for (Length i = 0; ok && i < length; i++) text += static_cast<char16_t>(Get<uint16_t>());
			return Printable(GeneralUtils::UTF16ToWTF8(text));
		}
		template<typename Length> std::string Text() {
			const auto length = Get<Length>();
			std::string text;
			for (Length i = 0; ok && i < length; i++) text += static_cast<char>(Get<uint8_t>());
			return Printable(text);
		}

		// Narrow text is shown as is when it is UTF-8, else byte by byte as Latin-1 (JSON only takes UTF-8)
		static std::string Printable(const std::string& text) {
			if (IsUtf8(text)) return text;
			std::string out;
			for (const auto c : text) {
				const auto byte = static_cast<uint8_t>(c);
				if (byte < 0x80) out += c;
				else {
					out += static_cast<char>(0xC0 | (byte >> 6));
					out += static_cast<char>(0x80 | (byte & 0x3F));
				}
			}
			return out;
		}
	};

	template<typename E>
	json Enum(uint64_t value) {
		const auto name = magic_enum::enum_name(static_cast<E>(value));
		return name.empty() ? json(value) : json(std::string(name) + " (" + std::to_string(value) + ")");
	}

	// Larger compressed LDF is not inflated (a construction's config is a few hundred bytes)
	constexpr uint32_t MAX_LDF_BYTES = 1024 * 1024;

	// LDF entries as the client reads them: "key=type:value"
	json ReadLdfEntries(Reader& r, int32_t count) {
		json out = json::array();
		for (int32_t i = 0; r.ok && i < count && i < 4096; i++) {
			const auto keyBytes = r.Get<uint8_t>();
			std::u16string key;
			for (uint8_t c = 0; r.ok && c < keyBytes / 2; c++) key += static_cast<char16_t>(r.Get<uint16_t>());
			const auto type = r.Get<uint8_t>();
			std::string value;
			switch (type) {
			case 0: value = r.WideText<uint32_t>(); break;
			case 1: value = std::to_string(r.Get<int32_t>()); break;
			case 3: value = std::to_string(r.Get<float>()); break;
			case 4: value = std::to_string(r.Get<double>()); break;
			case 5: value = std::to_string(r.Get<uint32_t>()); break;
			case 7: value = std::to_string(r.Get<uint8_t>()); break;
			case 8: value = std::to_string(r.Get<uint64_t>()); break;
			case 9: value = std::to_string(r.Get<int64_t>()); break;
			case 13: value = r.Text<uint32_t>(); break;
			default: r.ok = false; break;
			}
			out.push_back(Reader::Printable(GeneralUtils::UTF16ToWTF8(key)) + "=" + std::to_string(type) + ":" + value);
		}
		return out;
	}

	/**
	 * u32 size, u8 compressed, then the entries (a u32 count and each entry). Compressed: u32 uncompressed size, u32
	 * compressed size and that many bytes of zlib data holding the entries, inflated here.
	 */
	json ReadLdf(Reader& r) {
		const auto size = r.Get<uint32_t>();
		const auto compressed = r.Get<uint8_t>();
		if (!compressed) return ReadLdfEntries(r, r.Get<int32_t>());
		const auto uncompressedSize = r.Get<uint32_t>();
		const auto compressedSize = r.Get<uint32_t>();
		std::string data;
		for (uint32_t i = 0; r.ok && i < compressedSize; i++) data += static_cast<char>(r.Get<uint8_t>());
		json out{ {"compressed", true}, {"size", size}, {"uncompressedSize", uncompressedSize}, {"compressedSize", compressedSize} };
		if (!r.ok || uncompressedSize > 1024 * 1024) return out;
		std::string entries(uncompressedSize, '\0');
		int32_t error = 0;
		const auto inflated = ZCompression::Decompress(reinterpret_cast<const uint8_t*>(data.data()), compressedSize,
			reinterpret_cast<uint8_t*>(entries.data()), uncompressedSize, error);
		if (inflated != static_cast<int32_t>(uncompressedSize)) {
			out["(did not inflate)"] = true;
			return out;
		}
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(entries.data()), uncompressedSize, false);
		Reader inner{ stream };
		out["entries"] = ReadLdfEntries(inner, inner.Get<int32_t>());
		if (!inner.ok || stream.GetNumberOfUnreadBits() > 0) out["(entries did not read)"] = true;
		return out;
	}

	// Activity user info: object ID and 10 values each
	json ActivityPlayers(Reader& r) {
		json players = json::array();
		const auto count = r.Get<uint32_t>();
		for (uint32_t i = 0; r.ok && i < count && i < 256; i++) {
			json values = json::array();
			const auto player = r.Id();
			for (int v = 0; v < 10; v++) values.push_back(r.Get<float>());
			players.push_back({ {"player", player}, {"values", values} });
		}
		return players;
	}

	// A part the server never writes (only live did), whose layout isn't known here: stop instead of guessing
	void NotRead(Reader& r, json& j, const char* what) {
		j["(" + std::string(what) + " present, not read)"] = true;
		r.ok = false;
	}

	// PhysicsComponent::Serialize
	void Position(Reader& r, json& j) {
		if (r.Bit()) {
			j["position"] = r.Point();
			j["rotation"] = r.Rotation();
		}
	}

	// The object a character stands on and where on it (LWOBasePhysComponent's frame stats)
	json LocalSpace(Reader& r) {
		json out{ {"object", r.Id()}, {"position", r.Point()} };
		if (r.Bit()) out["velocity"] = r.Point();
		return out;
	}

	// LWOBuffComponent::ReadBuffs: a u32 count and each buff
	json Buffs(Reader& r) {
		json buffs = json::array();
		const auto count = r.Get<uint32_t>();
		for (uint32_t i = 0; r.ok && i < count && i < 256; i++) {
			json buff{ {"id", r.Get<uint32_t>()} };
			if (r.Bit()) buff["timeMs"] = r.Get<uint32_t>();
			for (const char* flag : { "cancelOnDeath", "cancelOnZone", "cancelOnDamaged", "cancelOnRemoveBuff", "cancelOnUi", "cancelOnLogout", "cancelOnUnequip", "cancelOnDamageAbsorbRanOut" }) buff[flag] = r.Bit();
			const bool addedByTeammate = r.Bit();
			buff["addedByTeammate"] = addedByTeammate;
			buff["applyOnTeammates"] = r.Bit();
			if (addedByTeammate) buff["source"] = r.Id();
			buff["refCount"] = r.Get<uint32_t>();
			buffs.push_back(buff);
		}
		return buffs;
	}

	void ModelBase(Reader& r, json& j) {
		if (!r.Bit()) return;
		j["pickable"] = r.Bit();
		j["modelType"] = r.Get<uint32_t>();
		j["originalPosition"] = r.Point();
		j["originalRotation"] = r.RawRotation();
	}

	using ComponentReader = std::function<void(Reader&, json&, bool initial, const std::vector<eReplicaComponentType>& components)>;

	// One reader per component, each mirroring the component's Serialize(bIsInitialUpdate)
	const std::map<eReplicaComponentType, ComponentReader>& Readers() {
		static const std::map<eReplicaComponentType, ComponentReader> readers{
			{ POSSESSABLE, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				if (r.Bit()) j["possessor"] = r.Id();
				if (r.Bit()) j["animationFlag"] = r.Get<uint32_t>();
				j["immediatelyDepossess"] = r.Bit();
			} },
			{ MODULE_ASSEMBLY, [](Reader& r, json& j, bool initial, const auto&) {
				if (!initial || !r.Bit()) return;
				if (r.Bit()) j["subKey"] = r.Id();
				j["useOptionalParts"] = r.Bit();
				j["assemblyPartLOTs"] = r.WideText<uint16_t>();
			} },
			{ CONTROLLABLE_PHYSICS, [](Reader& r, json& j, bool initial, const auto&) {
				if (initial) {
					if ((j["inJetpackMode"] = r.Bit()).get<bool>()) {
						j["jetpackEffectID"] = r.Get<int32_t>();
						j["jetpackFlying"] = r.Bit();
						j["jetpackBypassChecks"] = r.Bit();
					}
					if (r.Bit()) {
						json stun = json::array();
						for (int i = 0; i < 7; i++) stun.push_back(r.Get<int32_t>());
						j["immuneToStunCounts"] = stun; // move, jump, turn, attack, use item, equip, interact
					}
				}
				if (r.Bit()) {
					j["gravityScale"] = r.Get<float>();
					j["speedMultiplier"] = r.Get<float>();
				}
				if (r.Bit()) {
					j["pickupRadius"] = r.Get<float>();
					j["inJetpackModeEquipped"] = r.Bit();
				}
				if (r.Bit()) {
					if ((j["inBubble"] = r.Bit()).get<bool>()) {
						j["bubbleType"] = r.Get<uint32_t>();
						j["specialAnims"] = r.Bit();
					}
				}
				if (r.Bit()) {
					j["position"] = r.Point();
					j["rotation"] = r.Rotation();
					j["onGround"] = r.Bit();
					j["onRail"] = r.Bit();
					if (r.Bit()) j["velocity"] = r.Point();
					if (r.Bit()) j["angularVelocity"] = r.Point();
					if (r.Bit()) j["localSpace"] = LocalSpace(r);
					if (!initial) j["teleporting"] = r.Bit();
				}
			} },
			{ SIMPLE_PHYSICS, [](Reader& r, json& j, bool initial, const auto&) {
				if (initial) {
					j["climbable"] = r.Bit();
					j["climbableType"] = r.Get<int32_t>();
				}
				if (r.Bit()) {
					j["velocity"] = r.Point();
					j["angularVelocity"] = r.Point();
				}
				if (r.Bit()) j["motionType"] = r.Get<uint32_t>();
				Position(r, j);
			} },
			{ RIGID_BODY_PHANTOM_PHYSICS, [](Reader& r, json& j, bool, const auto&) { Position(r, j); } },
			{ HAVOK_VEHICLE_PHYSICS, [](Reader& r, json& j, bool initial, const auto&) {
				if (r.Bit()) {
					j["position"] = r.Point();
					j["rotation"] = r.Rotation();
					j["onGround"] = r.Bit();
					j["onRail"] = r.Bit();
					if (r.Bit()) j["velocity"] = r.Point();
					if (r.Bit()) j["angularVelocity"] = r.Point();
					if (r.Bit()) j["localSpace"] = LocalSpace(r);
					if (r.Bit()) {
						j["remoteInputX"] = r.Get<float>();
						j["remoteInputY"] = r.Get<float>();
						j["powersliding"] = r.Bit();
						j["modified"] = r.Bit();
						j["remoteInputPing"] = r.Get<float>();
					}
					if (!initial) j["teleporting"] = r.Bit();
				}
				if (initial) {
					j["endBehavior"] = r.Get<uint8_t>();
					j["inputLocked"] = r.Bit();
				}
				if (r.Bit()) NotRead(r, j, "trailingFlag");
			} },
			{ PHANTOM_PHYSICS, [](Reader& r, json& j, bool, const auto&) {
				Position(r, j);
				if (!r.Bit()) return;
				if (!(j["effectActive"] = r.Bit()).get<bool>()) return;
				j["effectType"] = r.Get<uint32_t>();
				j["directionalMultiplier"] = r.Get<float>();
				if (r.Bit()) {
					j["minDistance"] = r.Get<float>();
					j["maxDistance"] = r.Get<float>();
				}
				if ((j["directional"] = r.Bit()).get<bool>()) j["direction"] = r.Point();
			} },
			{ SOUND_TRIGGER, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				json cues = json::array();
				for (uint8_t n = r.Get<uint8_t>(), i = 0; r.ok && i < n; i++) {
					const auto name = r.Text<uint8_t>();
					const auto result = r.Get<uint32_t>();
					cues.push_back({ {"name", name}, {"result", result}, {"boredomTime", r.Get<float>()} });
				}
				j["musicCues"] = cues;
				json parameters = json::array();
				for (uint8_t n = r.Get<uint8_t>(), i = 0; r.ok && i < n; i++) {
					const auto name = r.Text<uint8_t>();
					parameters.push_back({ {"name", name}, {"value", r.Get<float>()} });
				}
				j["musicParameters"] = parameters;
				for (const char* key : { "ambientSounds2D", "ambientSounds3D" }) {
					json sounds = json::array();
					for (uint8_t n = r.Get<uint8_t>(), i = 0; r.ok && i < n; i++) {
						const auto data1 = r.Get<uint32_t>();
						const auto data2 = r.Get<uint16_t>();
						const auto data3 = r.Get<uint16_t>();
						std::string data4;
						for (int b = 0; b < 8; b++) data4 += std::to_string(r.Get<uint8_t>()) + (b < 7 ? "," : "");
						sounds.push_back({ {"guid", std::to_string(data1) + "-" + std::to_string(data2) + "-" + std::to_string(data3) + "-" + data4}, {"result", r.Get<uint32_t>()} });
					}
					j[key] = sounds;
				}
				json mixers = json::array();
				for (uint8_t n = r.Get<uint8_t>(), i = 0; r.ok && i < n; i++) {
					const auto name = r.Text<uint8_t>();
					mixers.push_back({ {"name", name}, {"result", r.Get<uint32_t>()} });
				}
				j["mixerPrograms"] = mixers;
			} },
			{ BUFF, [](Reader& r, json& j, bool initial, const auto&) {
				if (!initial) return;
				if (r.Bit()) j["buffs"] = Buffs(r);
				if (r.Bit()) j["immunities"] = Buffs(r);
			} },
			{ DESTROYABLE, [](Reader& r, json& j, bool initial, const auto&) {
				if (initial && r.Bit()) {
					json immunities = json::object();
					for (const char* key : { "basicAttack", "damageOverTime", "knockback", "interrupt", "speed", "imaginationGain", "imaginationLoss", "quickbuildInterrupt", "pullToPoint" }) immunities[key] = r.Get<uint32_t>();
					j["immuneToCounts"] = immunities;
				}
				if (r.Bit()) {
					j["health"] = r.Get<int32_t>();
					j["maxHealth"] = r.Get<float>();
					j["armor"] = r.Get<int32_t>();
					j["maxArmor"] = r.Get<float>();
					j["imagination"] = r.Get<int32_t>();
					j["maxImagination"] = r.Get<float>();
					j["damageAbsorptionPoints"] = r.Get<int32_t>();
					j["immune"] = r.Bit();
					j["gmImmune"] = r.Bit();
					j["shielded"] = r.Bit();
					j["actualMaxHealth"] = r.Get<float>();
					j["actualMaxArmor"] = r.Get<float>();
					j["actualMaxImagination"] = r.Get<float>();
					json factions = json::array();
					const auto count = r.Get<uint32_t>();
					for (uint32_t i = 0; r.ok && i < count && i < 256; i++) factions.push_back(r.Get<int32_t>());
					j["factions"] = factions;
					const bool smashable = r.Bit();
					j["smashable"] = smashable;
					if (initial) {
						j["dead"] = r.Bit();
						j["smashed"] = r.Bit();
						if (smashable) {
							j["moduleAssembly"] = r.Bit();
							if (r.Bit()) j["explodeFactor"] = r.Get<float>();
						}
					}
				}
				if (r.Bit()) j["onThreatList"] = r.Bit();
			} },
			{ COLLECTIBLE, [](Reader& r, json& j, bool, const auto&) { j["collectibleID"] = r.Get<int16_t>(); } },
			// LWOPetComponent::Deserialize: everything under the dirty bit, the names on updates too
			{ PET, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["status"] = r.Get<uint32_t>();
				j["ability"] = r.Get<uint32_t>();
				if (r.Bit()) j["interaction"] = r.Id();
				if (r.Bit()) j["owner"] = r.Id();
				if (r.Bit()) {
					j["moderationStatus"] = r.Get<uint32_t>();
					j["name"] = r.WideText<uint8_t>();
					j["ownerName"] = r.WideText<uint8_t>();
				}
			} },
			{ POSSESSOR, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				if (r.Bit()) j["possessable"] = r.Id();
				j["possessableType"] = r.Get<uint8_t>();
			} },
			{ LEVEL_PROGRESSION, [](Reader& r, json& j, bool, const auto&) { if (r.Bit()) j["level"] = r.Get<uint32_t>(); } },
			{ PLAYER_FORCED_MOVEMENT, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["onRail"] = r.Bit();
				j["showBillboard"] = r.Bit();
			} },
			{ CHARACTER, [](Reader& r, json& j, bool initial, const auto&) {
				if (initial) {
					json claimCodes = json::array();
					for (int i = 0; i < 4; i++) claimCodes.push_back(r.Bit() ? json(std::to_string(r.Get<uint64_t>())) : json(nullptr));
					j["claimCodes"] = claimCodes;
					for (const char* key : { "hairColor", "hairStyle", "head", "shirtColor", "pantsColor", "shirtStyle", "headColor", "eyebrows", "eyes", "mouth" }) j[key] = r.Get<uint32_t>();
					j["accountID"] = std::to_string(r.Get<uint64_t>());
					j["lastLogin"] = std::to_string(r.Get<uint64_t>());
					j["propModLastDisplayTime"] = std::to_string(r.Get<uint64_t>());
					j["uscore"] = std::to_string(r.Get<uint64_t>());
					j["freeToPlay"] = r.Bit();
					json stats = json::array();
					for (int i = 0; i < 27; i++) stats.push_back(std::to_string(r.Get<uint64_t>()));
					j["statistics"] = stats;
					j["unknownFlag"] = r.Bit();
					if ((j["landing"] = r.Bit()).get<bool>()) j["lastRocketConfig"] = r.WideText<uint16_t>();
				}
				if (r.Bit()) {
					j["pvpEnabled"] = r.Bit();
					j["isGM"] = r.Bit();
					j["gmLevel"] = r.Get<uint8_t>();
					j["editorEnabled"] = r.Bit();
					j["editorLevel"] = r.Get<uint8_t>();
				}
				if (r.Bit()) j["currentActivity"] = r.Get<uint32_t>();
				if (r.Bit()) {
					j["guildID"] = r.Id();
					j["guildName"] = r.WideText<uint8_t>();
					j["legoClubMember"] = r.Bit();
					j["countryCode"] = r.Get<int32_t>();
				}
			} },
			{ INVENTORY, [](Reader& r, json& j, bool initial, const auto&) {
				if (r.Bit()) {
					json items = json::array();
					const auto count = r.Get<uint32_t>();
					for (uint32_t i = 0; r.ok && i < count && i < 1024; i++) {
						json item{ {"id", r.Id()}, {"lot", r.Get<int32_t>()} };
						if (r.Bit()) item["subkey"] = r.Id();
						if (r.Bit()) item["count"] = r.Get<uint32_t>();
						if (r.Bit()) item["slot"] = r.Get<uint16_t>();
						if (r.Bit()) item["inventoryType"] = r.Get<uint32_t>();
						if (r.Bit()) item["config"] = ReadLdf(r);
						item["bound"] = r.Bit();
						items.push_back(item);
					}
					j["equipped"] = items;
				}
				if (r.Bit()) j["equippedModelTransforms"] = r.Get<uint32_t>();
			} },
			{ SCRIPT, [](Reader& r, json& j, bool initial, const auto&) {
				if (initial && r.Bit()) j["networkSettings"] = ReadLdf(r);
			} },
			{ SKILL, [](Reader& r, json& j, bool initial, const auto&) {
				// LWOSkillComponent::Deserialize: the skills being cast and each one's running behaviors
				if (!initial || !r.Bit()) return;
				json skills = json::array();
				const auto count = r.Get<uint32_t>();
				for (uint32_t i = 0; r.ok && i < count && i < 256; i++) {
					json skill{ {"skillUID", r.Get<uint32_t>()} };
					skill["skillID"] = r.Get<uint32_t>();
					skill["castType"] = r.Get<uint32_t>();
					skill["cancelType"] = r.Get<uint32_t>();
					json behaviors = json::array();
					const auto behaviorCount = r.Get<uint32_t>();
					for (uint32_t b = 0; r.ok && b < behaviorCount && b < 256; b++) {
						json behavior{ {"behaviorHandle", r.Get<uint32_t>()} };
						behavior["action"] = r.Get<uint32_t>();
						behavior["waitTimeMs"] = r.Get<uint32_t>();
						behavior["templateID"] = r.Get<uint32_t>();
						behavior["caster"] = r.Id();
						behavior["originator"] = r.Id();
						behavior["target"] = r.Id();
						behavior["usedMouse"] = r.Bit();
						behavior["cooldown"] = r.Get<float>();
						behavior["chargeTime"] = r.Get<float>();
						behavior["imaginationCost"] = r.Get<float>();
						behaviors.push_back(behavior);
					}
					skill["behaviors"] = behaviors;
					skills.push_back(skill);
				}
				j["skillsInProgress"] = skills;
			} },
			{ BASE_COMBAT_AI, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["state"] = r.Get<uint32_t>();
				j["target"] = r.Id();
			} },
			{ ITEM, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["ugID"] = r.Id();
				j["ugModerationStatus"] = r.Get<uint32_t>();
				if (r.Bit()) j["ugDescription"] = r.WideText<uint32_t>();
			} },
			{ QUICK_BUILD, [](Reader& r, json& j, bool initial, const auto&) {
				if (r.Bit()) j["players"] = ActivityPlayers(r);
				if (r.Bit()) {
					j["state"] = r.Get<uint32_t>();
					j["success"] = r.Bit();
					j["enabled"] = r.Bit();
					j["timeSinceStart"] = r.Get<float>();
					j["pausedTime"] = r.Get<float>();
					if (initial) {
						// LWOQuickBuildComponent::Deserialize: a choice build has a u32 of its settings after the bit
						if ((j["choiceBuild"] = r.Bit()).get<bool>()) j["choiceBuildSetting"] = r.Get<uint32_t>();
						j["activatorPosition"] = r.Point();
						j["repositionPlayer"] = r.Bit();
					}
				}
			} },
			// LWOMovingPlatformComponent::Deserialize: the path when dirty, then each subcomponent (a 1 bit before
			// each, a 0 bit after the last) as its type
			{ MOVING_PLATFORM, [](Reader& r, json& j, bool, const auto&) {
				const bool hasSubcomponents = r.Bit();
				if (r.Bit() && r.Bit()) {
					j["pathName"] = r.WideText<uint16_t>();
					j["startingWaypoint"] = r.Get<uint32_t>();
					j["reverse"] = r.Bit();
				}
				if (!hasSubcomponents) return;
				json subcomponents = json::array();
				while (r.ok && subcomponents.size() < 16 && r.Bit()) {
					const auto type = r.Get<uint32_t>();
					json sub{ {"type", type} };
					if (type == 4) {
						// LWOPlatformMover
						if (r.Bit()) {
							sub["state"] = r.Get<uint32_t>();
							sub["desiredWaypoint"] = r.Get<int32_t>();
							sub["stopAtDesiredWaypoint"] = r.Bit();
							sub["reverse"] = r.Bit();
							sub["percentBetweenPoints"] = r.Get<float>();
							sub["position"] = r.Point();
							sub["currentWaypoint"] = r.Get<uint32_t>();
							sub["nextWaypoint"] = r.Get<uint32_t>();
							sub["idleTimeElapsed"] = r.Get<float>();
							sub["moveTimeElapsed"] = r.Get<float>();
						}
					} else if (type == 5) {
						// LWOPlatformSimpleMover
						if (r.Bit() && r.Bit()) {
							sub["startPosition"] = r.Point();
							sub["startRotation"] = r.Rotation();
						}
						if (r.Bit()) {
							sub["state"] = r.Get<uint32_t>();
							sub["currentWaypoint"] = r.Get<uint32_t>();
							sub["reverse"] = r.Bit();
						}
					} else {
						NotRead(r, sub, "subcomponent");
					}
					subcomponents.push_back(sub);
				}
				j["subcomponents"] = subcomponents;
			} },
			{ SWITCH, [](Reader& r, json& j, bool, const auto&) { j["active"] = r.Bit(); } },
			{ VENDOR, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["hasStandardCostItems"] = r.Bit();
				j["hasMultiCostItems"] = r.Bit();
			} },
			{ DONATION_VENDOR, [](Reader& r, json& j, bool, const auto&) {
				if (r.Bit()) {
					j["hasStandardCostItems"] = r.Bit();
					j["hasMultiCostItems"] = r.Bit();
				}
				if (!r.Bit()) return;
				j["percentComplete"] = r.Get<float>();
				j["totalDonated"] = r.Get<int32_t>();
				j["totalRemaining"] = r.Get<int32_t>();
			} },
			{ ACHIEVEMENT_VENDOR, [](Reader& r, json& j, bool, const auto&) {
				if (!r.Bit()) return;
				j["hasStandardCostItems"] = r.Bit();
				j["hasMultiCostItems"] = r.Bit();
			} },
			{ BOUNCER, [](Reader& r, json& j, bool, const auto&) {
				if ((j["petEnabled"] = r.Bit()).get<bool>()) j["petBouncerEnabled"] = r.Bit();
			} },
			{ SCRIPTED_ACTIVITY, [](Reader& r, json& j, bool, const auto&) { if (r.Bit()) j["players"] = ActivityPlayers(r); } },
			{ SHOOTING_GALLERY, [](Reader& r, json& j, bool initial, const auto&) {
				if (r.Bit()) j["players"] = ActivityPlayers(r);
				if (initial) {
					j["cameraPosition"] = r.Point();
					j["cameraLookatPosition"] = r.Point();
				}
				if (!r.Bit()) return;
				j["cannonVelocity"] = r.Get<double>();
				j["cannonRefireRate"] = r.Get<double>();
				j["cannonMinDistance"] = r.Get<double>();
				j["cameraBarrelOffset"] = r.Point();
				j["cannonAngle"] = r.Get<float>();
				j["facing"] = r.Point();
				j["currentPlayer"] = r.Id();
				j["cannonTimeout"] = r.Get<float>();
				j["cannonFOV"] = r.Get<float>();
			} },
			{ RACING_CONTROL, [](Reader& r, json& j, bool, const auto&) {
				if (r.Bit()) j["players"] = ActivityPlayers(r);
				if (r.Bit()) {
					j["expectedPlayers"] = r.Get<uint16_t>();
					if (r.Bit()) {
						json loading = json::array();
						while (r.ok && r.Bit()) {
							json p{ {"player", r.Id()}, {"vehicle", r.Id()} };
							p["index"] = r.Get<uint32_t>();
							p["loaded"] = r.Bit();
							loading.push_back(p);
						}
						j["preRacePlayers"] = loading;
					}
				}
				if (r.Bit()) {
					json finished = json::array();
					while (r.ok && r.Bit()) {
						json p{ {"player", r.Id()} };
						p["finished"] = r.Get<uint32_t>();
						finished.push_back(p);
					}
					j["postRacePlayers"] = finished;
				}
				if (r.Bit()) {
					j["remainingLaps"] = r.Get<uint16_t>();
					j["pathName"] = r.WideText<uint16_t>();
				}
				if (r.Bit()) {
					json results = json::array();
					while (r.ok && r.Bit()) {
						json p{ {"player", r.Id()} };
						p["bestLapTime"] = r.Get<float>();
						p["raceTime"] = r.Get<float>();
						results.push_back(p);
					}
					j["results"] = results;
				}
			} },
			{ LUP_EXHIBIT, [](Reader& r, json& j, bool, const auto&) { if (r.Bit()) j["exhibitLOT"] = r.Get<int32_t>(); } },
			// LWOModelBehaviorComponent::Deserialize (the user-generated-content block before it is the item's)
			{ MODEL, [](Reader& r, json& j, bool, const auto&) { ModelBase(r, j); } },
			// LWOMutableModelBehaviorComponent::Deserialize: the model's block, the behaviors, and on construction who
			// is editing it
			{ MUTABLE_MODEL_BEHAVIORS, [](Reader& r, json& j, bool initial, const auto&) {
				ModelBase(r, j);
				if (r.Bit()) {
					j["behaviors"] = r.Get<uint32_t>();
					j["paused"] = r.Bit();
				}
				if (initial && r.Bit()) {
					j["oldObjectID"] = r.Id();
					j["editor"] = r.Id();
				}
			} },
			// LWOBBBComponent::Deserialize (characters carry one in live captures)
			{ BBB, [](Reader& r, json& j, bool, const auto&) { if (r.Bit()) j["metadataSourceItem"] = r.Id(); } },
			// LWOTriggerComponent::Deserialize, on objects whose header sets the trigger bit
			{ TRIGGER, [](Reader& r, json& j, bool, const auto&) { if (r.Bit()) j["triggerID"] = r.Get<int32_t>(); } },
			{ RENDER, [](Reader& r, json& j, bool initial, const auto&) {
				if (!initial) return;
				json effects = json::array();
				const auto count = r.Get<uint32_t>();
				for (uint32_t i = 0; r.ok && i < count && i < 1024; i++) {
					const auto name = r.Text<uint8_t>();
					if (name.empty()) {
						effects.push_back(json::object());
						continue;
					}
					json effect{ {"name", name}, {"effectID", r.Get<int32_t>()} };
					effect["type"] = r.WideText<uint8_t>();
					effect["priority"] = r.Get<float>();
					effect["secondary"] = r.Id();
					effects.push_back(effect);
				}
				j["effects"] = effects;
			} },
			{ MINI_GAME_CONTROL, [](Reader& r, json& j, bool, const auto&) { j["value"] = r.Get<uint32_t>(); } },
		};
		return readers;
	}

	// The order the client reads components in (Entity.cpp SERIALIZATION_ORDER), with the character's parts in front
	constexpr std::array ORDER{
		POSSESSABLE, MODULE_ASSEMBLY, CONTROLLABLE_PHYSICS, SIMPLE_PHYSICS, RIGID_BODY_PHANTOM_PHYSICS, HAVOK_VEHICLE_PHYSICS,
		PHANTOM_PHYSICS, SOUND_TRIGGER, RACING_SOUND_TRIGGER, BUFF, DESTROYABLE, COLLECTIBLE, PET, POSSESSOR, LEVEL_PROGRESSION,
		PLAYER_FORCED_MOVEMENT, CHARACTER, INVENTORY, SCRIPT, SKILL, BASE_COMBAT_AI, ITEM, QUICK_BUILD, MOVING_PLATFORM, SWITCH,
		VENDOR, DONATION_VENDOR, ACHIEVEMENT_VENDOR, BOUNCER, SCRIPTED_ACTIVITY, SHOOTING_GALLERY, RACING_CONTROL, LUP_EXHIBIT,
		MODEL, MUTABLE_MODEL_BEHAVIORS, RENDER, MINI_GAME_CONTROL, BBB,
	};

	/**
	 * The components as Entity::Initialize makes them from the registry, in the order Entity::WriteComponents writes
	 * them (the destroyable where DestroyableSerializationSlot puts it). `extraDestroyable`: one the registry doesn't
	 * list (is_smashable objects, models).
	 */
	std::vector<eReplicaComponentType> Arrange(std::set<eReplicaComponentType> has, bool extraDestroyable) {
		if (has.contains(DESTROYABLE)) has.insert(BUFF);
		if (has.contains(CHARACTER)) has.insert({ POSSESSOR, LEVEL_PROGRESSION, PLAYER_FORCED_MOVEMENT });
		// The client drops a pet's model and item components (ObjectLoader2::DoObjectComponentLoad)
		if (has.contains(PET)) {
			has.erase(MODEL);
			has.erase(MUTABLE_MODEL_BEHAVIORS);
			has.erase(ITEM);
		}
		// Objects listing component 107 (characters) write a BBB component's data after the others in live captures
		if (has.contains(CRAFTING)) has.insert(BBB);
		// Collectibles get one; a quick build without one writes the same empty bits itself in the same place
		const bool destroyable = has.contains(DESTROYABLE) || has.contains(COLLECTIBLE) || has.contains(QUICK_BUILD) || extraDestroyable;
		eReplicaComponentType slot = MINI_GAME_CONTROL;
		if (has.contains(BUFF) || has.contains(COLLECTIBLE)) slot = DESTROYABLE;
		else if (has.contains(QUICK_BUILD)) slot = QUICK_BUILD;
		has.erase(DESTROYABLE);
		std::vector<eReplicaComponentType> out;
		for (const auto type : ORDER) {
			if (destroyable && type == slot) out.push_back(DESTROYABLE);
			if (type != DESTROYABLE && has.contains(type)) out.push_back(type);
		}
		return out;
	}

	std::set<eReplicaComponentType> Registered(LOT lot, const ReplicaDecoder::ComponentTable& table) {
		// BBB models (LOT 14) are made up in code: simple physics, model, render and a destroyable after them
		if (lot == 14) return { SIMPLE_PHYSICS, ITEM, MODEL, RENDER };
		const auto it = table.find(lot);
		if (it == table.end()) return {};
		return { it->second.begin(), it->second.end() };
	}

	// The layouts to try for an object, the registry's own first
	/**
	 * `mutableModel`: the object's config has propertyObjectID or inInventory set, for which the client makes a model's
	 * mutable component instead of the plain one (ObjectLoader2::LoadModelBehaviorsComponent)
	 */
	std::vector<std::vector<eReplicaComponentType>> Candidates(LOT lot, const ReplicaDecoder::ComponentTable& table, bool mutableModel) {
		auto has = Registered(lot, table);
		if (mutableModel && has.contains(MODEL)) {
			has.erase(MODEL);
			has.insert(MUTABLE_MODEL_BEHAVIORS);
		}
		// Models get a destroyable too (Entity::Initialize), BBB models always
		const bool model = (has.contains(MODEL) || has.contains(MUTABLE_MODEL_BEHAVIORS)) && !has.contains(PET);
		std::vector<std::vector<eReplicaComponentType>> out{ Arrange(has, lot == 14 || model) };
		auto add = [&out](std::vector<eReplicaComponentType> candidate) {
			if (std::find(out.begin(), out.end(), candidate) == out.end()) out.push_back(std::move(candidate));
		};
		// Set up by the zone file rather than the registry: a smashable's destroyable, a moving platform's path, a script
		add(Arrange(has, true));
		auto withPlatform = has;
		withPlatform.insert(MOVING_PLATFORM);
		add(Arrange(withPlatform, false));
		add(Arrange(withPlatform, true));
		auto withScript = has;
		withScript.insert(SCRIPT);
		add(Arrange(withScript, false));
		add(Arrange(withScript, true));
		auto withoutScript = has;
		withoutScript.erase(SCRIPT);
		add(Arrange(withoutScript, false));
		// A simple physics object the zone file sets markedAsPhantom on gets phantom physics instead
		// (LWOSimplePhysicsComponent::Allocator)
		if (has.contains(SIMPLE_PHYSICS)) {
			auto phantom = has;
			phantom.erase(SIMPLE_PHYSICS);
			phantom.insert(PHANTOM_PHYSICS);
			add(Arrange(phantom, false));
			add(Arrange(phantom, true));
		}
		// The client makes no FX component (the render data) when the zone file sets renderDisabled, as on trigger
		// volumes (ObjectLoader2::LoadRenderComponent)
		if (has.contains(RENDER)) {
			auto withoutRender = has;
			withoutRender.erase(RENDER);
			add(Arrange(withoutRender, false));
			add(Arrange(withoutRender, true));
		}
		return out;
	}

	std::string Name(eReplicaComponentType type) {
		const auto name = magic_enum::enum_name(type);
		return name.empty() ? std::to_string(static_cast<uint32_t>(type)) : std::string(name);
	}

	std::string RestHex(RakNet::BitStream& stream) {
		std::string out;
		static constexpr char digits[] = "0123456789abcdef";
		const auto offset = stream.GetReadOffset();
		while (stream.GetNumberOfUnreadBits() >= 8) {
			uint8_t byte{};
			stream.Read(byte);
			out += digits[byte >> 4];
			out += digits[byte & 15];
		}
		const auto bits = stream.GetNumberOfUnreadBits();
		if (bits > 0) {
			out += " +";
			for (uint32_t i = 0; i < bits; i++) {
				bool bit{};
				stream.Read(bit);
				out += bit ? '1' : '0';
			}
		}
		stream.SetReadOffset(offset);
		return out;
	}

	// What is left after the last component: padding (under a byte, all zero) or data no reader took
	bool OnlyPadding(RakNet::BitStream& stream) {
		const auto unread = stream.GetNumberOfUnreadBits();
		if (unread >= 8) return false;
		const auto offset = stream.GetReadOffset();
		bool zero = true;
		for (uint32_t i = 0; i < unread; i++) {
			bool bit{};
			stream.Read(bit);
			zero = zero && !bit;
		}
		stream.SetReadOffset(offset);
		return zero;
	}

	// Reads the components in order from `start`; true when they read the stream exactly
	bool ReadComponents(RakNet::BitStream& stream, BitSize_t start, std::vector<eReplicaComponentType> components, bool trigger, bool initial, json& out) {
		stream.SetReadOffset(start);
		out = json::array();
		Reader r{ stream };
		// Made for objects with a trigger after all the others (ObjectLoader2::DoObjectComponentLoad), so read last
		if (trigger) components.push_back(TRIGGER);
		for (const auto type : components) {
			json fields = json::object();
			const auto reader = Readers().find(type);
			if (reader == Readers().end()) continue;
			const auto before = stream.GetReadOffset();
			reader->second(r, fields, initial, components);
			if (!r.ok) {
				stream.SetReadOffset(before);
				out.push_back({ {"component", Name(type)}, {"fields", fields}, {"(did not read)", true} });
				return false;
			}
			out.push_back({ {"component", Name(type)}, {"fields", fields} });
		}
		return OnlyPadding(stream);
	}

	// Whether a construction's config (entries as "key=type:value") names a property or says it is in an inventory
	bool MutableModel(const json& config) {
		const auto& entries = config.is_object() ? config.value("entries", json::array()) : config;
		if (!entries.is_array()) return false;
		for (const auto& entry : entries) {
			if (!entry.is_string()) continue;
			const auto text = entry.get<std::string>();
			if (text.starts_with("propertyObjectID=") || text == "inInventory=7:1") return true;
		}
		return false;
	}

	json ReadParentChild(Reader& r) {
		json out = json::object();
		if (!r.Bit()) return out;
		if (r.Bit()) {
			out["parent"] = r.Id();
			out["updatePositionWithParent"] = r.Bit();
		}
		if (r.Bit()) {
			json children = json::array();
			const auto count = r.Get<uint16_t>();
			for (uint16_t i = 0; r.ok && i < count; i++) children.push_back(r.Id());
			out["children"] = children;
		}
		return out;
	}
}

namespace ReplicaDecoder {
	size_t LoadComponentTable(ComponentTable& table) {
		table.clear();
		auto result = CDClientDatabase::ExecuteQuery("SELECT id, component_type FROM ComponentsRegistry");
		while (!result.eof()) {
			table[result.getIntField(0)].push_back(static_cast<eReplicaComponentType>(result.getIntField(1)));
			result.nextRow();
		}
		return table.size();
	}

	std::vector<eReplicaComponentType> ComponentsOf(LOT lot, const ComponentTable& table) {
		return Candidates(lot, table, false).front();
	}

	std::optional<json> Session::Decode(std::string_view bytes, uint64_t connection) {
		if (bytes.empty()) return std::nullopt;
		const auto id = static_cast<uint8_t>(bytes[0]);
		if (id != ID_REPLICA_MANAGER_CONSTRUCTION && id != ID_REPLICA_MANAGER_SERIALIZE && id != ID_REPLICA_MANAGER_DESTRUCTION) return std::nullopt;
		RakNet::BitStream stream(reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data())), static_cast<unsigned int>(bytes.size()), false);
		stream.IgnoreBytes(1);
		Reader r{ stream };
		json out = json::object();

		if (id == ID_REPLICA_MANAGER_DESTRUCTION) {
			const auto network = r.Get<uint16_t>();
			if (!r.ok) return json{ {"(did not read)", true} };
			out["networkID"] = network;
			const auto it = m_Objects.find({ connection, network });
			if (it != m_Objects.end()) {
				out["objectID"] = std::to_string(it->second.objectId);
				out["lot"] = it->second.lot;
				m_Objects.erase(it);
			}
			return out;
		}

		if (id == ID_REPLICA_MANAGER_SERIALIZE) {
			const auto network = r.Get<uint16_t>();
			out["networkID"] = network;
			const auto it = m_Objects.find({ connection, network });
			if (it == m_Objects.end()) {
				out["(object not constructed in this capture)"] = true;
				out["(rest)"] = RestHex(stream);
				return out;
			}
			out["objectID"] = std::to_string(it->second.objectId);
			out["lot"] = it->second.lot;
			out["parentChild"] = ReadParentChild(r);
			json components;
			const auto start = stream.GetReadOffset();
			if (!r.ok || !ReadComponents(stream, start, it->second.components, it->second.trigger, false, components)) {
				out["(layout did not match)"] = true;
				out["(rest)"] = RestHex(stream);
			}
			out["components"] = components;
			return out;
		}

		// Construction: Entity::WriteBaseReplicaData, then the components
		r.Bit();
		const auto network = r.Get<uint16_t>();
		const auto objectId = r.Get<int64_t>();
		const auto lot = r.Get<int32_t>();
		out["networkID"] = network;
		out["objectID"] = std::to_string(objectId);
		out["lot"] = lot;
		out["name"] = r.WideText<uint8_t>();
		out["timeSinceCreatedMs"] = r.Get<uint32_t>();
		if (r.Bit()) out["config"] = ReadLdf(r);
		const bool trigger = r.Bit();
		out["trigger"] = trigger;
		if (r.Bit()) out["spawner"] = r.Id();
		if (r.Bit()) out["spawnerNode"] = r.Get<uint32_t>();
		if (r.Bit()) out["scale"] = r.Get<float>();
		if (r.Bit()) out["worldState"] = r.Get<uint8_t>();
		if (r.Bit()) out["gmLevel"] = r.Get<uint8_t>();
		out["parentChild"] = ReadParentChild(r);
		if (!r.ok) {
			out["(did not read)"] = true;
			return out;
		}

		const auto start = stream.GetReadOffset();
		const auto candidates = Candidates(lot, m_Table, out.contains("config") && MutableModel(out["config"]));
		std::vector<eReplicaComponentType> chosen = candidates.front();
		json components;
		bool matched = false;
		for (const auto& candidate : candidates) {
			json attempt;
			if (ReadComponents(stream, start, candidate, trigger, true, attempt)) {
				chosen = candidate;
				components = std::move(attempt);
				matched = true;
				break;
			}
		}
		if (!matched) {
			// Shown as the registry says, as far as it reads, with the rest as bytes
			ReadComponents(stream, start, chosen, trigger, true, components);
			out["(layout did not match)"] = true;
			out["(rest)"] = RestHex(stream);
		}
		if (!m_Table.contains(lot) && lot != 14) out["(LOT not in ComponentsRegistry)"] = true;
		out["components"] = components;
		m_Objects[{ connection, network }] = Object{ objectId, lot, chosen, trigger };
		return out;
	}
}
