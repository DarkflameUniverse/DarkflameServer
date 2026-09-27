#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "tinyxml2.h"
#include "ContrabandRules.h"
#include "GeneralUtils.h"
#include "dCommonVars.h"
#include "eCharacterVersion.h"
#include "eInventoryType.h"

/**
 * Checks a character XML uploaded on the dashboard before it is stored. That upload is the one place hand-written
 * XML reaches the game, and the game's load path (Character::DoQuickXMLDataParse and the LoadFromXml/LoadXml of the
 * Character, Inventory, Mission, Destroyable, Buff, LevelProgression, ControllablePhysics and Character components)
 * trusts the XML because the server writes it itself. So instead of the game skipping bad data at load (and saving
 * the loss back), bad uploads are refused here.
 *
 * - errors: things the load path would throw on, read uninitialized, stop loading at (dropping everything after it,
 *   which is then saved back), or silently drop (an unknown mission or item, a duplicate item id or slot).
 *   The upload is refused.
 * - warnings: valid but suspicious content (contraband, stacks above the item's stack size, coins, level or u-score
 *   out of reach, GM level above the account's). Staff see them and must confirm to store the XML.
 *
 * Pure (the game data it needs comes in through Lookups) so it can be unit tested.
 */
namespace CharacterXmlCheck {
	constexpr int64_t MAX_COINS = 2000000000;
	constexpr int64_t MAX_COUNT = 999999;
	constexpr size_t MAX_REPORTED = 50;

	// Game data from the CDClient; an unset lookup skips its check
	struct Lookups {
		std::function<bool(LOT)> isItem;                            // the LOT has an item component
		std::function<int32_t(LOT)> stackSize;                      // the item's stack size, 0 if unknown
		std::function<bool(int32_t)> missionExists;                 // the mission is in the Missions table
		std::function<std::optional<int64_t>(uint32_t)> levelUScore; // u-score needed to reach a level
		uint32_t maxLevel = 0;                                       // highest level (0: unknown)
	};

	struct Context {
		uint32_t ownerAccountId = 0;   // the account of the character being replaced
		int32_t accountGmLevel = 0;    // that account's GM level
		bool contrabandApplies = true; // Contraband::Applies for that account
		Contraband::List contraband;
		Lookups lookups;
	};

	struct Result {
		std::vector<std::string> errors;
		std::vector<std::string> warnings;
		std::vector<Contraband::Finding> contraband;
		bool Ok() const { return errors.empty(); }
	};

	namespace Detail {
		enum class eKind { INT, UINT, INT64, UINT64, FLOAT, BOOL };

		inline const char* KindName(eKind kind) {
			switch (kind) {
			case eKind::INT: return "a whole number";
			case eKind::UINT: return "a non-negative whole number";
			case eKind::INT64: return "a whole number";
			case eKind::UINT64: return "a non-negative whole number";
			case eKind::FLOAT: return "a number";
			case eKind::BOOL: return "true/false or 1/0";
			}
			return "a number";
		}

		// Parses like the game's tinyxml2 Query*Attribute calls
		inline bool Parses(const tinyxml2::XMLElement& e, const char* name, eKind kind) {
			int i; unsigned u; int64_t i64; uint64_t u64; float f; bool b;
			switch (kind) {
			case eKind::INT: return e.QueryIntAttribute(name, &i) == tinyxml2::XML_SUCCESS;
			case eKind::UINT: return e.QueryUnsignedAttribute(name, &u) == tinyxml2::XML_SUCCESS && e.Attribute(name)[0] != '-';
			case eKind::INT64: return e.QueryInt64Attribute(name, &i64) == tinyxml2::XML_SUCCESS;
			case eKind::UINT64: return e.QueryUnsigned64Attribute(name, &u64) == tinyxml2::XML_SUCCESS && e.Attribute(name)[0] != '-';
			case eKind::FLOAT: return e.QueryFloatAttribute(name, &f) == tinyxml2::XML_SUCCESS;
			case eKind::BOOL: return e.QueryBoolAttribute(name, &b) == tinyxml2::XML_SUCCESS;
			}
			return false;
		}

		struct Checker {
			Result& result;
			const Context& context;

			void Error(const std::string& text) { if (result.errors.size() < MAX_REPORTED) result.errors.push_back(text); }
			void Warn(const std::string& text) { if (result.warnings.size() < MAX_REPORTED) result.warnings.push_back(text); }

			// Optional attributes: when present they must parse
			void Numbers(const tinyxml2::XMLElement& e, const std::string& where, eKind kind, std::initializer_list<const char*> names) {
				for (const auto* name : names) {
					const char* value = e.Attribute(name);
					if (value && !Parses(e, name, kind)) Error(where + ": " + name + "=\"" + value + "\" must be " + KindName(kind));
				}
			}

			// Required attributes: the game reads them into uninitialized variables
			bool Required(const tinyxml2::XMLElement& e, const std::string& where, eKind kind, std::initializer_list<const char*> names) {
				bool ok = true;
				for (const auto* name : names) {
					if (!e.Attribute(name)) { Error(where + ": " + name + " is missing"); ok = false; }
				}
				Numbers(e, where, kind, names);
				for (const auto* name : names) if (e.Attribute(name) && !Parses(e, name, kind)) ok = false;
				return ok;
			}

			static int64_t Int64(const tinyxml2::XMLElement& e, const char* name) { return e.Int64Attribute(name, 0); }
			static std::string Str(int64_t value) { return std::to_string(value); }

			static bool KnownInventory(uint32_t type) { return type <= eInventoryType::ITEM_SETS; }
			static bool KnownMissionState(int64_t state) {
				switch (state) {
				case -1: case 0: case 1: case 2: case 4: case 8: case 9: case 10: case 12: case 16: return true;
				default: return false;
				}
			}

			void Mf(const tinyxml2::XMLElement& mf) {
				Numbers(mf, "<mf>", eKind::UINT, { "hc", "hs", "hd", "t", "l", "hdc", "cd", "lh", "rh", "es", "ess", "ms" });
			}

			void Char(const tinyxml2::XMLElement& c) {
				const std::string where = "<char>";
				Numbers(c, where, eKind::INT64, { "cc", "rpt", "ls", "lrid", "gid", "llog" });
				Numbers(c, where, eKind::INT, { "gm", "ft" });
				Numbers(c, where, eKind::UINT, { "lwid", "lnzid", "acct" });
				Numbers(c, where, eKind::UINT64, { "lzid", "co", "co1", "co2", "co3", "time" });
				Numbers(c, where, eKind::FLOAT, { "lzx", "lzy", "lzz", "lzrx", "lzry", "lzrz", "lzrw" });

				if (c.Attribute("acct") && Parses(c, "acct", eKind::UINT) && c.UnsignedAttribute("acct") != context.ownerAccountId) {
					Error(where + ": acct=\"" + c.Attribute("acct") + "\" is not the account of this character (" + std::to_string(context.ownerAccountId) + ")");
				}
				if (c.Attribute("gm") && Parses(c, "gm", eKind::INT)) {
					const auto gm = c.IntAttribute("gm");
					if (gm < 0 || gm > 9) Error(where + ": gm=\"" + std::to_string(gm) + "\" is not a GM level (0 to 9)");
					else if (gm > context.accountGmLevel) Warn("GM level " + std::to_string(gm) + " in the XML is above the account's (" + std::to_string(context.accountGmLevel) + ")");
				}
				if (c.Attribute("cc") && Parses(c, "cc", eKind::INT64)) {
					const auto coins = Int64(c, "cc");
					if (coins < 0) Error(where + ": cc (coins) can't be negative");
					else if (coins > MAX_COINS) Warn("Coins " + Str(coins) + " are above " + Str(MAX_COINS));
				}
				if (c.Attribute("ls") && Parses(c, "ls", eKind::INT64) && Int64(c, "ls") < 0) Error(where + ": ls (u-score) can't be negative");

				if (const auto* ue = c.FirstChildElement("ue")) {
					for (const auto* e = ue->FirstChildElement(); e; e = e->NextSiblingElement()) Required(*e, "<char><ue> emote", eKind::INT, { "id" });
				}
				if (const auto* vl = c.FirstChildElement("vl")) {
					for (const auto* l = vl->FirstChildElement("l"); l; l = l->NextSiblingElement("l")) Numbers(*l, "<char><vl><l>", eKind::INT, { "id", "cid" });
				}
				if (const auto* zs = c.FirstChildElement("zs")) {
					for (const auto* s = zs->FirstChildElement(); s; s = s->NextSiblingElement()) {
						Required(*s, "<char><zs> zone statistics", eKind::UINT, { "map" });
						Numbers(*s, "<char><zs> zone statistics", eKind::UINT64, { "ac", "cc", "es", "qbc" });
						Numbers(*s, "<char><zs> zone statistics", eKind::INT64, { "bc" });
					}
				}
			}

			void Flags(const tinyxml2::XMLElement& flags) {
				for (const auto* f = flags.FirstChildElement(); f; f = f->NextSiblingElement()) {
					const char* id = f->Attribute("id");
					const char* v = f->Attribute("v");
					// Character::DoQuickXMLDataParse reads both with std::stoul/std::stoull, which throw
					if (id && v) {
						if (!GeneralUtils::TryParse<uint32_t>(id)) Error(std::string("<flag>: id=\"") + id + "\" must be a non-negative whole number");
						if (!GeneralUtils::TryParse<uint64_t>(v)) Error(std::string("<flag>: v=\"") + v + "\" must be a non-negative whole number");
					}
				}
			}

			void Dest(const tinyxml2::XMLElement& dest) {
				Numbers(dest, "<dest>", eKind::INT, { "hc", "ic", "ac" });
				Numbers(dest, "<dest>", eKind::FLOAT, { "hm", "im", "am" });
				if (const auto* buff = dest.FirstChildElement("buff")) {
					for (const auto* b = buff->FirstChildElement("b"); b; b = b->NextSiblingElement("b")) {
						Numbers(*b, "<dest><buff><b>", eKind::INT, { "id", "b", "refCount" });
						Numbers(*b, "<dest><buff><b>", eKind::FLOAT, { "t", "tk", "tt", "s" });
						Numbers(*b, "<dest><buff><b>", eKind::INT64, { "sr" });
						Numbers(*b, "<dest><buff><b>", eKind::BOOL, { "cancelOnDamaged", "cancelOnDeath", "cancelOnLogout", "cancelOnRemoveBuff", "cancelOnUi", "cancelOnUnequip", "cancelOnZone", "applyOnTeammates" });
					}
				}
			}

			void Level(const tinyxml2::XMLElement& lvl, int64_t uscore) {
				Numbers(lvl, "<lvl>", eKind::UINT, { "l", "cv" });
				Numbers(lvl, "<lvl>", eKind::FLOAT, { "sb" });
				if (lvl.Attribute("cv") && Parses(lvl, "cv", eKind::UINT) && lvl.UnsignedAttribute("cv") > GeneralUtils::ToUnderlying(eCharacterVersion::UP_TO_DATE)) {
					Error("<lvl>: cv=\"" + std::string(lvl.Attribute("cv")) + "\" is newer than this server's character version (" + std::to_string(GeneralUtils::ToUnderlying(eCharacterVersion::UP_TO_DATE)) + ")");
				}
				if (!lvl.Attribute("l") || !Parses(lvl, "l", eKind::UINT)) return;
				const auto level = lvl.UnsignedAttribute("l");
				const auto& lookups = context.lookups;
				if (lookups.maxLevel > 0 && level > lookups.maxLevel) {
					Warn("Level " + std::to_string(level) + " is above the highest level (" + std::to_string(lookups.maxLevel) + ")");
					return;
				}
				if (!lookups.levelUScore || uscore < 0) return;
				const auto needed = lookups.levelUScore(level);
				if (needed && uscore < *needed) Warn("Level " + std::to_string(level) + " needs " + Str(*needed) + " u-score but the character has " + Str(uscore));
				const auto next = level < lookups.maxLevel ? lookups.levelUScore(level + 1) : std::nullopt;
				if (next && uscore >= *next) Warn("U-score " + Str(uscore) + " is enough for level " + std::to_string(level + 1) + " but the character is level " + std::to_string(level));
			}

			void Inventory(const tinyxml2::XMLElement& inv) {
				Numbers(inv, "<inv>", eKind::INT, { "csl" });
				if (const auto* grps = inv.FirstChildElement("grps")) {
					for (const auto* g = grps->FirstChildElement("grp"); g; g = g->NextSiblingElement("grp")) Numbers(*g, "<inv><grps><grp>", eKind::UINT, { "t" });
				}
				const auto* bag = inv.FirstChildElement("bag");
				for (const auto* b = bag->FirstChildElement(); b; b = b->NextSiblingElement()) {
					if (Required(*b, "<inv><bag><b>", eKind::UINT, { "t", "m" }) && !KnownInventory(b->UnsignedAttribute("t"))) {
						Error("<inv><bag><b>: t=\"" + std::string(b->Attribute("t")) + "\" is not an inventory type");
					}
				}

				std::set<LWOOBJID> ids;
				std::map<uint32_t, std::set<uint32_t>> slots;
				std::map<LOT, int32_t> stackSizes;
				const auto& lookups = context.lookups;
				for (const auto* in = inv.FirstChildElement("items")->FirstChildElement(); in; in = in->NextSiblingElement()) {
					if (!Required(*in, "<inv><items><in>", eKind::UINT, { "t" })) continue;
					const auto type = in->UnsignedAttribute("t");
					if (!KnownInventory(type)) {
						Error("<inv><items><in>: t=\"" + std::to_string(type) + "\" is not an inventory type");
						continue;
					}
					const auto inventoryName = std::string(InventoryType::InventoryTypeToString(static_cast<eInventoryType>(type)));
					// Items in the build inventories are moved (and given new slots) when the character loads
					const bool reslotted = type == eInventoryType::MODELS_IN_BBB || type == eInventoryType::BRICKS_IN_BBB;
					for (const auto* i = in->FirstChildElement(); i; i = i->NextSiblingElement()) {
						const std::string where = "Item in " + inventoryName + (i->Attribute("l") ? std::string(" (LOT ") + i->Attribute("l") + ")" : std::string());
						bool ok = Required(*i, where, eKind::INT64, { "id" });
						ok = Required(*i, where, eKind::INT, { "l" }) && ok;
						ok = Required(*i, where, eKind::UINT, { "s", "c" }) && ok;
						ok = Required(*i, where, eKind::BOOL, { "eq", "b" }) && ok;
						Numbers(*i, where, eKind::INT64, { "sk", "parent" });
						if (!ok) continue;

						const auto id = i->Int64Attribute("id");
						const LOT lot = i->IntAttribute("l");
						const auto slot = i->UnsignedAttribute("s");
						const auto count = i->UnsignedAttribute("c");
						if (id == LWOOBJID_EMPTY) Error(where + ": id can't be 0");
						else if (!ids.insert(id).second) Error(where + ": id " + std::to_string(id) + " is used by another item (the game would drop one)");
						if (!reslotted && !slots[type].insert(slot).second) Error(where + ": slot " + std::to_string(slot) + " is used by another item in " + inventoryName + " (the game would drop one)");
						if (lookups.isItem && !lookups.isItem(lot)) {
							Error(where + ": LOT " + std::to_string(lot) + " is not an item");
							continue;
						}
						if (count == 0) Warn(where + ": count is 0");
						else if (count > MAX_COUNT) Warn(where + ": count " + std::to_string(count) + " is above " + std::to_string(MAX_COUNT));
						else if (lookups.stackSize) {
							auto it = stackSizes.find(lot);
							if (it == stackSizes.end()) it = stackSizes.emplace(lot, lookups.stackSize(lot)).first;
							if (it->second > 0 && count > static_cast<uint32_t>(it->second)) Warn(where + ": count " + std::to_string(count) + " is above the stack size (" + std::to_string(it->second) + ")");
						}
						// Proxy items of item sets aren't what the player holds, like the world's check
						if (i->Int64Attribute("parent", LWOOBJID_EMPTY) == LWOOBJID_EMPTY) held.push_back({ id, lot, count, static_cast<eInventoryType>(type) });
					}
				}
			}

			void Pets(const tinyxml2::XMLElement& pet) {
				for (const auto* p = pet.FirstChildElement(); p; p = p->NextSiblingElement()) {
					Numbers(*p, "<pet> pet", eKind::INT64, { "id" });
					Numbers(*p, "<pet> pet", eKind::INT, { "l", "m", "t" });
				}
			}

			void Missions(const tinyxml2::XMLElement& mis) {
				const auto& lookups = context.lookups;
				const auto mission = [&](const tinyxml2::XMLElement& m, const char* where, bool done) {
					if (!Required(m, where, eKind::INT, { "id" })) return;
					const auto id = m.IntAttribute("id");
					const std::string name = std::string(where) + " " + std::to_string(id);
					if (lookups.missionExists && !lookups.missionExists(id)) Error(name + ": no such mission (the game would lose it)");
					Numbers(m, name, eKind::INT, { "state" });
					Numbers(m, name, eKind::UINT, { "cct", "cts", "o" });
					if (m.Attribute("state") && Parses(m, "state", eKind::INT)) {
						const auto state = m.IntAttribute("state");
						if (!KnownMissionState(state)) Error(name + ": state " + std::to_string(state) + " is not a mission state");
						else if (done && state < 8) Warn(name + " is in the completed list but its state (" + std::to_string(state) + ") is not complete");
					}
					if (!done) for (const auto* t = m.FirstChildElement(); t; t = t->NextSiblingElement()) Numbers(*t, name + " task", eKind::UINT, { "v" });
				};
				if (const auto* done = mis.FirstChildElement("done")) for (const auto* m = done->FirstChildElement(); m; m = m->NextSiblingElement()) mission(*m, "Completed mission", true);
				if (const auto* cur = mis.FirstChildElement("cur")) for (const auto* m = cur->FirstChildElement(); m; m = m->NextSiblingElement()) mission(*m, "Current mission", false);
			}

			void Respawns(const tinyxml2::XMLElement& res) {
				for (const auto* r = res.FirstChildElement("r"); r; r = r->NextSiblingElement("r")) {
					Numbers(*r, "<res><r>", eKind::INT, { "w" });
					Numbers(*r, "<res><r>", eKind::FLOAT, { "x", "y", "z" });
				}
			}

			std::vector<Contraband::HeldItem> held;
		};
	}

	inline Result Check(const std::string& xml, const Context& context) {
		Result result;
		Detail::Checker check{ result, context };
		tinyxml2::XMLDocument doc;
		if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
			check.Error(xml.empty() ? "The XML is empty" : std::string("That is not valid XML: ") + (doc.ErrorStr() ? doc.ErrorStr() : "parse error"));
			return result;
		}
		const auto* obj = doc.FirstChildElement("obj");
		if (!obj) {
			check.Error("The root element must be <obj>");
			return result;
		}

		// Loading stops at the first of these that is missing; what would have been read after it is lost when the character is saved
		const auto* character = obj->FirstChildElement("char");
		const auto* mf = obj->FirstChildElement("mf");
		const auto* inv = obj->FirstChildElement("inv");
		const auto* items = inv ? inv->FirstChildElement("items") : nullptr;
		if (!character) check.Error("<obj> must have a <char> element");
		if (!mf) check.Error("<obj> must have an <mf> (appearance) element");
		if (!inv) check.Error("<obj> must have an <inv> element");
		if (inv && !inv->FirstChildElement("bag")) check.Error("<inv> must have a <bag> element");
		if (inv && (!items || !items->FirstChildElement("in"))) check.Error("<inv> must have an <items> element with at least one <in>");

		if (mf) check.Mf(*mf);
		int64_t uscore = -1;
		if (character) {
			check.Char(*character);
			if (character->Attribute("ls") && Detail::Parses(*character, "ls", Detail::eKind::INT64)) uscore = character->Int64Attribute("ls");
			else uscore = 0;
		}
		if (const auto* flags = obj->FirstChildElement("flag")) check.Flags(*flags);
		if (const auto* dest = obj->FirstChildElement("dest")) check.Dest(*dest);
		if (const auto* lvl = obj->FirstChildElement("lvl")) check.Level(*lvl, uscore);
		if (inv && inv->FirstChildElement("bag") && items) check.Inventory(*inv);
		if (const auto* pet = obj->FirstChildElement("pet")) check.Pets(*pet);
		if (const auto* mis = obj->FirstChildElement("mis")) check.Missions(*mis);
		if (const auto* res = obj->FirstChildElement("res")) check.Respawns(*res);

		if (context.contrabandApplies) {
			result.contraband = Contraband::Find(check.held, context.contraband);
			for (const auto& found : result.contraband) {
				const bool remove = found.entry.action == IContraband::eContrabandAction::REMOVE;
				check.Warn("Contraband: " + std::to_string(found.item.count) + " of item " + std::to_string(found.item.lot) + " (id " + std::to_string(found.item.id) + ") in " +
					InventoryType::InventoryTypeToString(found.item.inventory) + (remove ? " [flag and remove]" : " [flag]") + (found.entry.reason.empty() ? "" : ": " + found.entry.reason));
			}
		}
		return result;
	}

	// The XML without the items with these ids (and without set proxy items whose parent is one of them), printed compactly
	inline std::string RemoveItems(const std::string& xml, const std::set<LWOOBJID>& ids) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return xml;
		auto* inv = doc.FirstChildElement("obj") ? doc.FirstChildElement("obj")->FirstChildElement("inv") : nullptr;
		auto* items = inv ? inv->FirstChildElement("items") : nullptr;
		for (auto* in = items ? items->FirstChildElement() : nullptr; in; in = in->NextSiblingElement()) {
			for (auto* i = in->FirstChildElement(); i;) {
				auto* next = i->NextSiblingElement();
				if (ids.contains(i->Int64Attribute("id")) || ids.contains(i->Int64Attribute("parent", LWOOBJID_EMPTY))) in->DeleteChild(i);
				i = next;
			}
		}
		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return printer.CStr();
	}
}
