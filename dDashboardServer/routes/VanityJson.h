#pragma once

#include <optional>
#include <string>

#include "json.hpp"
#include "LDFFormat.h"
#include "VanityXml.h"

/**
 * Vanity NPCs as JSON for the dashboard's vanity editor. The files themselves are read and written by VanityXml
 * (dCommon), the same reader the world servers use. Pure so it can be unit tested.
 */
namespace VanityJson {
	using VanityXml::Object;
	using VanityXml::Location;

	inline nlohmann::json ToJson(const Object& object) {
		nlohmann::json locations = nlohmann::json::array();
		for (const auto& l : object.locations) {
			nlohmann::json location{ {"zone", l.zone}, {"x", l.x}, {"y", l.y}, {"z", l.z}, {"rw", l.rw}, {"rx", l.rx}, {"ry", l.ry}, {"rz", l.rz} };
			if (l.chance) location["chance"] = *l.chance;
			if (l.scale) location["scale"] = *l.scale;
			locations.push_back(location);
		}
		return { {"name", object.name}, {"lot", object.lot}, {"equipment", object.equipment}, {"phrases", object.phrases}, {"config", object.config}, {"locations", locations} };
	}

	// One object from the page, checked the way the world server will read it
	inline std::optional<Object> FromJson(const nlohmann::json& json, std::string& error) {
		Object object;
		const auto fail = [&error](std::string message) { error = std::move(message); return std::nullopt; };
		if (!json.is_object()) return fail("Each NPC is an object");
		// A name is optional: the world spawns nameless objects too (demo.xml's plaque and trees)
		object.name = json.value("name", "");
		if (object.name.size() > 100) return fail("Names are up to 100 characters");
		const auto who = object.name.empty() ? std::string("An unnamed NPC: ") : "\"" + object.name + "\": ";
		object.lot = json.value("lot", 0);
		if (object.lot <= 0) return fail(who + "pick a LOT");
		for (const auto& lot : json.value("equipment", nlohmann::json::array())) {
			if (!lot.is_number_integer() || lot.get<int64_t>() <= 0) return fail(who + "equipment is item LOTs");
			object.equipment.push_back(lot.get<int32_t>());
		}
		for (const auto& phrase : json.value("phrases", nlohmann::json::array())) {
			if (!phrase.is_string() || phrase.get<std::string>().empty()) continue;
			object.phrases.push_back(phrase.get<std::string>());
		}
		for (const auto& key : json.value("config", nlohmann::json::array())) {
			if (!key.is_string() || key.get<std::string>().empty()) continue;
			const auto text = key.get<std::string>();
			// LDF: name=type:value, e.g. custom_script_client=0:scripts\ai\SPEC\MISSION_MINIGAME_CLIENT.lua; checked by the
			// parser the world uses on it (LwoNameValue::ParseInsert)
			if (text.starts_with('=') || !LDFBaseData::DataFromString(text)) {
				return fail(who + "config \"" + text + "\" should look like name=type:value");
			}
			object.config.push_back(text);
		}
		const auto locations = json.value("locations", nlohmann::json::array());
		if (locations.empty()) return fail(who + "add at least one location");
		for (const auto& l : locations) {
			const auto number = [&l](const char* name) -> std::optional<float> {
				if (!l.contains(name) || !l[name].is_number()) return std::nullopt;
				return l[name].get<float>();
			};
			const auto zone = l.value("zone", 0u);
			const auto x = number("x"), y = number("y"), z = number("z"), rw = number("rw"), rx = number("rx"), ry = number("ry"), rz = number("rz");
			if (zone == 0 || !x || !y || !z || !rw || !rx || !ry || !rz) return fail(who + "every location needs a zone and x, y, z and a rotation");
			Location location{ zone, *x, *y, *z, *rw, *rx, *ry, *rz, number("chance"), number("scale") };
			if (location.chance && (*location.chance < 0 || *location.chance > 1)) return fail(who + "chance is between 0 and 1");
			if (location.scale && *location.scale <= 0) return fail(who + "scale must be above 0");
			object.locations.push_back(location);
		}
		return object;
	}
}
