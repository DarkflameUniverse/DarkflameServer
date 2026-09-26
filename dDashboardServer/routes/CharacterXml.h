#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "json.hpp"
#include "tinyxml2.h"
#include "GeneralUtils.h"
#include "dCommonVars.h"
#include "eCharacterVersion.h"

/**
 * Reading and changing a character's saved XML for the dashboard's character editor and snapshot history.
 * The layout is the game's: <obj><char cc (coins) ls (u-score) .../><inv><bag><b t (inventory) m (size)/></bag>
 * <items><in t><i l (LOT) id s (slot) c (count) b (bound) eq (equipped) .../></in></items></inv><lvl l (level)/>...
 * Pure (no database or game data) so it can be unit tested.
 */
namespace CharacterXml {
	constexpr int64_t MAX_COINS = 2000000000;
	constexpr int64_t MAX_USCORE = 2000000000;
	constexpr int MAX_LEVEL = 45;
	constexpr int64_t MAX_COUNT = 999999;

	namespace Detail {
		inline int64_t Int(const tinyxml2::XMLElement* element, const char* name) {
			const char* value = element ? element->Attribute(name) : nullptr;
			return value ? GeneralUtils::TryParse<int64_t>(value).value_or(0) : 0;
		}

		inline tinyxml2::XMLElement* Inventory(tinyxml2::XMLElement* obj, uint32_t type) {
			auto* items = obj->FirstChildElement("inv") ? obj->FirstChildElement("inv")->FirstChildElement("items") : nullptr;
			if (!items) return nullptr;
			for (auto* in = items->FirstChildElement("in"); in; in = in->NextSiblingElement("in")) {
				if (static_cast<uint32_t>(in->IntAttribute("t", -1)) == type) return in;
			}
			return nullptr;
		}

		inline uint32_t InventorySize(tinyxml2::XMLElement* obj, uint32_t type) {
			auto* bag = obj->FirstChildElement("inv") ? obj->FirstChildElement("inv")->FirstChildElement("bag") : nullptr;
			for (auto* b = bag ? bag->FirstChildElement("b") : nullptr; b; b = b->NextSiblingElement("b")) {
				if (static_cast<uint32_t>(b->IntAttribute("t", -1)) == type) return static_cast<uint32_t>(b->IntAttribute("m", 0));
			}
			return 0;
		}
	}

	/**
	 * {coins, uscore, level, missions_done, inventories: [{type, size, items: [{id, lot, count, slot, bound, equipped}]}]},
	 * or null if the XML can't be read.
	 */
	inline nlohmann::json Summary(const std::string& xml) {
		tinyxml2::XMLDocument doc;
		if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return nullptr;
		auto* obj = doc.FirstChildElement("obj");
		if (!obj) return nullptr;
		const auto* character = obj->FirstChildElement("char");
		nlohmann::json inventories = nlohmann::json::array();
		auto* itemsRoot = obj->FirstChildElement("inv") ? obj->FirstChildElement("inv")->FirstChildElement("items") : nullptr;
		for (auto* in = itemsRoot ? itemsRoot->FirstChildElement("in") : nullptr; in; in = in->NextSiblingElement("in")) {
			const auto type = static_cast<uint32_t>(in->IntAttribute("t", 0));
			nlohmann::json items = nlohmann::json::array();
			for (auto* i = in->FirstChildElement("i"); i; i = i->NextSiblingElement("i")) {
				items.push_back({ {"id", std::to_string(Detail::Int(i, "id"))}, {"lot", Detail::Int(i, "l")}, {"count", Detail::Int(i, "c")},
					{"slot", Detail::Int(i, "s")}, {"bound", i->BoolAttribute("b")}, {"equipped", i->BoolAttribute("eq")} });
			}
			inventories.push_back({ {"type", type}, {"size", Detail::InventorySize(obj, type)}, {"items", items} });
		}
		size_t missions = 0;
		const auto* done = obj->FirstChildElement("mis") ? obj->FirstChildElement("mis")->FirstChildElement("done") : nullptr;
		for (const auto* m = done ? done->FirstChildElement("m") : nullptr; m; m = m->NextSiblingElement("m")) missions++;
		return {
			{"coins", Detail::Int(character, "cc")}, {"uscore", Detail::Int(character, "ls")},
			{"level", Detail::Int(obj->FirstChildElement("lvl"), "l")}, {"missions_done", missions}, {"inventories", inventories}
		};
	}

	/**
	 * Apply an edit: {coins, uscore, level, counts: {"itemId": count}, remove: ["itemId"], add: [{lot, count, inventory}]}.
	 * newId gives object IDs for added items. Returns the new XML (compact, as the game stores it), or nullopt and error.
	 */
	inline std::optional<std::string> Apply(const std::string& xml, const nlohmann::json& edit, const std::function<int64_t()>& newId, std::string& error) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS || !doc.FirstChildElement("obj")) { error = "The character's saved data can't be read"; return std::nullopt; }
		auto* obj = doc.FirstChildElement("obj");
		auto* character = obj->FirstChildElement("char");
		if (!character) { error = "The character's saved data has no <char> element"; return std::nullopt; }

		const auto number = [&](const char* key, int64_t min, int64_t max, int64_t& out) {
			if (!edit.contains(key) || edit[key].is_null()) return true;
			if (!edit[key].is_number_integer() || edit[key].get<int64_t>() < min || edit[key].get<int64_t>() > max) {
				error = std::string(key) + " must be a whole number from " + std::to_string(min) + " to " + std::to_string(max);
				return false;
			}
			out = edit[key].get<int64_t>();
			return true;
		};
		int64_t coins = -1, uscore = -1, level = -1;
		if (!number("coins", 0, MAX_COINS, coins) || !number("uscore", 0, MAX_USCORE, uscore) || !number("level", 1, MAX_LEVEL, level)) return std::nullopt;
		if (coins >= 0) character->SetAttribute("cc", coins);
		if (uscore >= 0) character->SetAttribute("ls", uscore);
		if (level >= 0) {
			auto* lvl = obj->FirstChildElement("lvl");
			if (!lvl) { error = "The character's saved data has no level"; return std::nullopt; }
			lvl->SetAttribute("l", level);
		}

		// Every item by id
		std::map<int64_t, tinyxml2::XMLElement*> items;
		auto* itemsRoot = obj->FirstChildElement("inv") ? obj->FirstChildElement("inv")->FirstChildElement("items") : nullptr;
		for (auto* in = itemsRoot ? itemsRoot->FirstChildElement("in") : nullptr; in; in = in->NextSiblingElement("in")) {
			for (auto* i = in->FirstChildElement("i"); i; i = i->NextSiblingElement("i")) items[Detail::Int(i, "id")] = i;
		}
		const auto findItem = [&](const std::string& id) -> tinyxml2::XMLElement* {
			const auto parsed = GeneralUtils::TryParse<int64_t>(id);
			const auto it = parsed ? items.find(*parsed) : items.end();
			if (it == items.end()) error = "The character has no item " + id;
			return it == items.end() ? nullptr : it->second;
		};

		if (edit.contains("counts") && edit["counts"].is_object()) {
			for (const auto& [id, count] : edit["counts"].items()) {
				auto* item = findItem(id);
				if (!item) return std::nullopt;
				if (!count.is_number_integer() || count.get<int64_t>() < 1 || count.get<int64_t>() > MAX_COUNT) { error = "Counts must be 1 to 999999"; return std::nullopt; }
				item->SetAttribute("c", count.get<int64_t>());
			}
		}
		if (edit.contains("remove") && edit["remove"].is_array()) {
			for (const auto& id : edit["remove"]) {
				auto* item = findItem(id.is_string() ? id.get<std::string>() : id.dump());
				if (!item) return std::nullopt;
				items.erase(Detail::Int(item, "id"));
				item->Parent()->DeleteChild(item);
			}
		}
		if (edit.contains("add") && edit["add"].is_array()) {
			for (const auto& add : edit["add"]) {
				const int64_t lot = add.value("lot", 0), count = add.value("count", 1);
				const auto type = add.value("inventory", 0u);
				if (lot <= 0) { error = "Pick an item to add"; return std::nullopt; }
				if (count < 1 || count > MAX_COUNT) { error = "Counts must be 1 to 999999"; return std::nullopt; }
				auto* in = Detail::Inventory(obj, type);
				if (!in) { error = "The character has no inventory " + std::to_string(type); return std::nullopt; }
				std::set<int64_t> used;
				for (auto* i = in->FirstChildElement("i"); i; i = i->NextSiblingElement("i")) used.insert(Detail::Int(i, "s"));
				const auto size = Detail::InventorySize(obj, type);
				int64_t slot = 0;
				while (used.contains(slot)) slot++;
				if (size > 0 && slot >= size) { error = "That inventory is full"; return std::nullopt; }
				auto* item = in->InsertNewChildElement("i");
				item->SetAttribute("l", lot);
				item->SetAttribute("id", newId());
				item->SetAttribute("s", slot);
				item->SetAttribute("c", count);
				item->SetAttribute("b", "false");
				item->SetAttribute("eq", "false");
				item->SetAttribute("sk", 0);
				item->SetAttribute("parent", 0);
			}
		}

		tinyxml2::XMLPrinter printer(nullptr, true);
		doc.Print(&printer);
		return std::string(printer.CStr());
	}

	struct Pet {
		LWOOBJID id{}; // as pet_names stores it
		int32_t lot{};
		std::string name; // as last saved on the character; pet_names has the current one
	};

	/**
	 * The character's pets: <pet><p id l n .../></pet>. Saves from before eCharacterVersion::PET_IDS hold ids with an
	 * extra bit 32 that the game clears when the character next logs in (the PET_IDS step in WorldServer.cpp); the same
	 * fix is applied here so the ids match pet_names.
	 */
	inline std::vector<Pet> Pets(const std::string& xml) {
		std::vector<Pet> pets;
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS || !doc.FirstChildElement("obj")) return pets;
		const auto* obj = doc.FirstChildElement("obj");
		const auto* lvl = obj->FirstChildElement("lvl");
		const bool oldIds = (lvl ? lvl->UnsignedAttribute("cv", 0) : 0) < static_cast<uint32_t>(eCharacterVersion::PET_IDS);
		const auto* section = obj->FirstChildElement("pet");
		for (const auto* p = section ? section->FirstChildElement("p") : nullptr; p; p = p->NextSiblingElement("p")) {
			LWOOBJID id = Detail::Int(p, "id");
			if (id == 0) continue;
			if (oldIds) id = GeneralUtils::ClearBit(id, 32);
			pets.push_back({ id, p->IntAttribute("l", 0), p->Attribute("n") ? p->Attribute("n") : "" });
		}
		return pets;
	}
}
