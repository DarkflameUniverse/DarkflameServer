#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "GeneralUtils.h"
#include "LevelFile.h"
#include "eReplicaComponentType.h"

/**
 * What the 3D world view draws of a zone besides its terrain: every object in its scene files (.lvl), read with LevelFile
 * (dCommon) like the world server's Level, sorted into a few kinds by the components their LOT has. Pure (bytes and
 * component lists in, objects and kinds out) so it can be unit tested; the route reads the files and the CDClient.
 */
namespace WorldScene {
	struct Object {
		LWOOBJID id{};
		uint32_t lot{};
		uint32_t templateLot{}; // what a spawner (LOT 176) spawns (spawntemplate); the object's own LOT otherwise
		float x{}, y{}, z{};
		float scale{ 1.0f };
		float qx{}, qy{}, qz{}, qw{ 1.0f }; // rotation
		bool spawner{};
		// The client draws no model for it (ObjectLoader2::LoadRenderComponent): renderDisabled or CreateNULLRender is
		// set, whatever the value (trigger volumes, markers)
		bool renderDisabled{};
		// carver_only: only carves the navigation mesh; the client never loads the object at all
		// (LWOResMgr2Interface::Run, 0x010610e5 in client 1.10.64), so it's never drawn (trigger volumes like LOT 5651)
		bool carverOnly{};
		std::string nifName;    // nif_name: the model to draw instead of its render component's
		bool clientOnly{};      // loadOnClientOnly: the client draws it, the world server never loads it
		std::string name;       // spawner_name, else respawnname, else empty
	};

	constexpr uint32_t SPAWNER_LOT = 176;

	// Every object in a scene file, as far as it could be read
	inline std::vector<Object> ReadObjects(const std::string& lvl) {
		std::istringstream stream(lvl);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			// A damaged file: keep the objects read before the damage
		}
		std::vector<Object> objects;
		objects.reserve(level.objects.size());
		for (const auto& object : level.objects) {
			const auto setting = [&object](const std::u16string& key) {
				const auto it = object.settings.find(key);
				auto value = it == object.settings.end() || !it->second ? std::string{} : it->second->GetValueAsString();
				while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) value.pop_back();
				return value;
			};
			Object out{ object.id, static_cast<uint32_t>(object.lot), static_cast<uint32_t>(object.lot),
				object.position.x, object.position.y, object.position.z, object.scale,
				object.rotation.x, object.rotation.y, object.rotation.z, object.rotation.w };
			out.spawner = object.lot == SPAWNER_LOT;
			if (out.spawner) out.templateLot = GeneralUtils::TryParse<uint32_t>(setting(u"spawntemplate")).value_or(0);
			out.clientOnly = GeneralUtils::TryParse(setting(u"loadOnClientOnly"), false);
			out.renderDisabled = object.settings.find(u"renderDisabled") != object.settings.end() || object.settings.find(u"CreateNULLRender") != object.settings.end();
			out.carverOnly = GeneralUtils::TryParse(setting(u"carver_only"), false);
			out.nifName = setting(u"nif_name");
			out.name = setting(u"spawner_name");
			if (out.name.empty()) out.name = setting(u"respawnname");
			objects.push_back(std::move(out));
		}
		return objects;
	}

	enum class eClientDraw : uint8_t {
		NO_MODEL, // nothing to draw: a spawner itself, or a model built from parts at run time
		HIDDEN,   // it has a model, but the client doesn't draw it (volumes, triggers, markers)
		DRAWN
	};

	/**
	 * Whether the client draws an object's model, given its LOT's Objects.type. As the client's
	 * ObjectLoader2::LoadRenderComponent (0x01053ca5 in client 1.10.64): nothing for renderDisabled or CreateNULLRender,
	 * blocking volumes and a few LOTs it leaves out by number (6368 is the 3D ambient sound, which gets an effect
	 * instead); and objects marked carver_only are never loaded at all (see carverOnly).
	 */
	inline eClientDraw ClientDraws(const Object& object, const std::string& type) {
		static constexpr std::array<uint32_t, 7> NOT_DRAWN{ 5937, 5938, 9741, 9742, 9862, 9863, 6368 };
		const auto lot = object.spawner ? object.templateLot : object.lot;
		if (lot == SPAWNER_LOT || lot == 0 || type == "PrimitiveModels") return eClientDraw::NO_MODEL;
		if (object.carverOnly || object.renderDisabled || type == "BlockingVolume" || std::find(NOT_DRAWN.begin(), NOT_DRAWN.end(), lot) != NOT_DRAWN.end()) return eClientDraw::HIDDEN;
		return eClientDraw::DRAWN;
	}

	/**
	 * The sky of a scene file: the first file name of its environment chunk's skydome info (the model the client draws
	 * around the camera, e.g. mesh\env\env_sky_won_ag_property.nif), empty when it has none. The chunk starts with
	 * the file offsets of its lighting, skydome and editor settings; the skydome info is length-prefixed file names.
	 */
	inline std::string ReadSkydome(const std::string& lvl) {
		std::istringstream stream(lvl);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			// The chunk headers read before any damage are enough
		}
		const auto read32 = [&lvl](size_t offset, uint32_t& value) {
			if (offset > lvl.size() || lvl.size() - offset < 4) return false;
			std::memcpy(&value, lvl.data() + offset, 4);
			return true;
		};
		for (const auto& [id, header] : level.chunkHeaders) {
			if (id != LevelFile::SceneEnviroment) continue;
			uint32_t skydome{}, length{};
			if (!read32(header.startPosition + 4, skydome) || !read32(skydome, length) || length == 0 || length > 260 || skydome + 4 + length > lvl.size()) return {};
			return lvl.substr(skydome + 4, length);
		}
		return {};
	}

	/**
	 * A scene's lighting from its environment chunk, as the client's shaders get it (EnvironmentManager::SetLightEnv,
	 * 0x01088aa0 in client 1.10.64): g_ambientLight, g_lightColor (the sun's color), g_upperHemiLight and g_lightVec,
	 * the unit vector toward the sun (the file stores the direction the light shines in). Colors are 0..1.
	 */
	struct Lighting {
		std::array<float, 3> ambient{};
		std::array<float, 3> specular{};
		std::array<float, 3> upperHemi{};
		std::array<float, 3> light{};     // the sun's color
		std::array<float, 3> lightVec{};  // toward the sun, unit length
		std::array<float, 3> fogColor{};
		float fogNear{};                  // fog at the highest draw distance setting
		float fogFar{};

		bool operator==(const Lighting&) const = default;
	};

	/**
	 * The lighting of a scene file, read as the client's level_read_lighting_info (0x0102f8f0) does, gated by the
	 * file's version; nullopt when the file has none or ends early. Scenes older than version 36 have no sun color
	 * (the client keeps black there); ones before 31 no fog.
	 */
	inline std::optional<Lighting> ReadLighting(const std::string& lvl) {
		std::istringstream stream(lvl);
		LevelFile level;
		try {
			level.Read(stream);
		} catch (const std::exception&) {
			// The chunk headers read before any damage are enough
		}
		const auto info = level.chunkHeaders.find(LevelFile::FileInfo);
		const auto environment = level.chunkHeaders.find(LevelFile::SceneEnviroment);
		if (info == level.chunkHeaders.end() || environment == level.chunkHeaders.end()) return std::nullopt;
		const auto version = info->second.fileInfo.version;
		size_t at = environment->second.startPosition; // the chunk starts with the offset of its lighting
		bool ok = true;
		const auto u32 = [&]() {
			uint32_t value{};
			if (at > lvl.size() || lvl.size() - at < 4) {
				ok = false;
				return value;
			}
			std::memcpy(&value, lvl.data() + at, 4);
			at += 4;
			return value;
		};
		const auto f32 = [&]() {
			const auto bits = u32();
			float value{};
			std::memcpy(&value, &bits, 4);
			return value;
		};
		const auto read3 = [&](std::array<float, 3>& out) { for (auto& value : out) value = f32(); };
		at = u32();
		if (!ok || at == 0) return std::nullopt;

		Lighting lighting;
		if (version > 44) f32(); // how long the client blends to it
		read3(lighting.ambient);
		read3(lighting.specular);
		read3(lighting.upperHemi);
		std::array<float, 3> direction{};
		read3(direction);
		if (version > 30) {
			if (version < 39) {
				lighting.fogNear = f32();
				lighting.fogFar = f32();
			} else {
				// Two draw distance settings (lowest, then highest): fog near and far, post fog solid and fade, static
				// and dynamic object distance
				for (int i = 0; i < 6; i++) f32();
				lighting.fogNear = f32();
				lighting.fogFar = f32();
				for (int i = 0; i < 4; i++) f32();
				if (version > 39) {
					const auto cullGroups = u32(); // group id, min, max each
					if (!ok || cullGroups > (lvl.size() - at) / 12) return std::nullopt;
					at += static_cast<size_t>(cullGroups) * 12;
				}
			}
			read3(lighting.fogColor);
		}
		if (version > 35) read3(lighting.light);
		if (!ok) return std::nullopt;
		const auto length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
		if (length > 0.0f) for (int i = 0; i < 3; i++) lighting.lightVec[i] = -direction[i] / length;
		return lighting;
	}

	/**
	 * The lighting to draw a whole zone with: the client blends to each scene's lighting as the player walks in, so a
	 * view of the zone takes the one most of its objects are lit by (scenes as {lighting, how many objects}; ties go
	 * to the first). nullopt when no scene has any.
	 */
	inline std::optional<Lighting> ZoneLighting(const std::vector<std::pair<Lighting, size_t>>& scenes) {
		std::vector<std::pair<Lighting, size_t>> totals;
		for (const auto& [lighting, objects] : scenes) {
			const auto it = std::find_if(totals.begin(), totals.end(), [&lighting](const auto& total) { return total.first == lighting; });
			if (it == totals.end()) totals.emplace_back(lighting, objects);
			else it->second += objects;
		}
		if (totals.empty()) return std::nullopt;
		return std::max_element(totals.begin(), totals.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
	}

	/**
	 * The kinds objects are drawn as, most telling first: an enemy that also offers missions is an enemy. Anything with
	 * none of these components is "other" (scenery, volumes, markers). Names come from the enum (GameLabels).
	 */
	constexpr std::array<eReplicaComponentType, 8> KINDS{
		eReplicaComponentType::BASE_COMBAT_AI, eReplicaComponentType::VENDOR, eReplicaComponentType::MISSION_OFFER,
		eReplicaComponentType::QUICK_BUILD, eReplicaComponentType::ROCKET_LAUNCH, eReplicaComponentType::COLLECTIBLE,
		eReplicaComponentType::SCRIPTED_ACTIVITY, eReplicaComponentType::DESTROYABLE
	};

	// Index into KINDS of an object with these ComponentsRegistry component types, or KINDS.size() for "other"
	inline size_t Classify(const std::vector<uint32_t>& registryComponents) {
		size_t best = KINDS.size();
		for (const auto component : registryComponents) {
			const auto type = static_cast<eReplicaComponentType>(component);
			for (size_t i = 0; i < best; i++) {
				if (KINDS[i] == type) { best = i; break; }
			}
		}
		return best;
	}
}
