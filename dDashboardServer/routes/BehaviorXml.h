#pragma once

#include <functional>
#include <string>

#include "json.hpp"
#include "magic_enum.hpp"
#include "tinyxml2.h"
#include "BehaviorStates.h"
#include "GameLabels.h"
#include "PropertyBehaviorActions.h"

/**
 * Property model behaviors as the database stores them (see dGame/dPropertyBehaviors), for the property viewer:
 *   <Behavior id name ...><State id="0"><Strip><Position x y/><Action Type="OnInteract"/><Action Type="MoveRight"
 *     ValueParameterName="Distance" Value="5"/>...</Strip></State></Behavior>
 * The first action of a strip is its trigger. Pure; unit tested.
 */
namespace BehaviorXml {
	// A state's name from the game's enum: CIRCLE_STATE -> "Circle"
	inline std::string StateName(int id) {
		const auto name = magic_enum::enum_name(static_cast<BehaviorState>(id));
		if (name.empty()) return "State";
		auto words = GameLabels::Words(name);
		if (words.ends_with(" State")) words.resize(words.size() - 6);
		return words;
	}

	/**
	 * The blocks the server plays and how (PropertyBehaviorActions, which Strip.cpp runs on), for the viewer's behavior
	 * player: {triggers: [type], moves: {type: [axis, sign]}, spawns: {type: {lot, name}}, drops: {type: {lot, name}},
	 * stateChanges: {type: stateId}, others: [type], states: {id: name}, defaultSpeed}. itemName names a LOT.
	 */
	inline nlohmann::json Rules(const std::function<std::string(int32_t)>& itemName) {
		using namespace PropertyBehaviorActions;
		nlohmann::json rules{ {"triggers", nlohmann::json::array()}, {"moves", nlohmann::json::object()}, {"spawns", nlohmann::json::object()},
			{"drops", nlohmann::json::object()}, {"stateChanges", nlohmann::json::object()}, {"others", nlohmann::json::array()},
			{"states", nlohmann::json::object()}, {"defaultSpeed", DEFAULT_SPEED} };
		for (const auto type : TRIGGERS) rules["triggers"].push_back(type);
		for (const auto& move : MOVES) rules["moves"][std::string(move.type)] = { std::string(1, move.axis), move.sign };
		for (const auto& spawn : SPAWNS) rules["spawns"][std::string(spawn.type)] = { {"lot", spawn.lot}, {"name", itemName(spawn.lot)} };
		for (const auto& drop : DROPS) rules["drops"][std::string(drop.type)] = { {"lot", drop.lot}, {"name", itemName(drop.lot)} };
		for (const auto& change : STATE_CHANGES) rules["stateChanges"][std::string(change.type)] = static_cast<int>(change.state);
		for (const auto type : OTHERS) rules["others"].push_back(type);
		for (const auto state : magic_enum::enum_values<BehaviorState>()) rules["states"][std::to_string(static_cast<int>(state))] = StateName(static_cast<int>(state));
		return rules;
	}

	// {id, name, states: [{id, name, strips: [{x, y, actions: [{type, parameter, value}]}]}]}, or null if unreadable
	inline nlohmann::json Parse(const std::string& xml) {
		tinyxml2::XMLDocument doc;
		if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return nullptr;
		const auto* behavior = doc.FirstChildElement("Behavior");
		if (!behavior) return nullptr;
		nlohmann::json states = nlohmann::json::array();
		for (const auto* state = behavior->FirstChildElement("State"); state; state = state->NextSiblingElement("State")) {
			const int stateId = state->IntAttribute("id", 0);
			nlohmann::json strips = nlohmann::json::array();
			for (const auto* strip = state->FirstChildElement("Strip"); strip; strip = strip->NextSiblingElement("Strip")) {
				nlohmann::json actions = nlohmann::json::array();
				for (const auto* action = strip->FirstChildElement("Action"); action; action = action->NextSiblingElement("Action")) {
					nlohmann::json entry{ {"type", action->Attribute("Type") ? action->Attribute("Type") : ""} };
					if (const auto* parameter = action->Attribute("ValueParameterName")) entry["parameter"] = parameter;
					if (const auto* value = action->Attribute("Value")) entry["value"] = value;
					actions.push_back(std::move(entry));
				}
				const auto* position = strip->FirstChildElement("Position");
				strips.push_back({ {"x", position ? position->DoubleAttribute("x") : 0.0}, {"y", position ? position->DoubleAttribute("y") : 0.0}, {"actions", actions} });
			}
			states.push_back({ {"id", stateId}, {"name", StateName(stateId)}, {"strips", strips} });
		}
		return { {"id", behavior->Attribute("id") ? behavior->Attribute("id") : ""}, {"name", behavior->Attribute("name") ? behavior->Attribute("name") : ""}, {"states", states} };
	}
}
