#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "UgcRender.h"

/**
 * Everything about an icon's framing and lighting that can be set, listed once: its name (in presets and overrides),
 * its setting (the shipped default, in ugcconfig.ini and the dashboard's settings), label, unit, range and default.
 * The UGC server reads the settings through it, the dashboard builds its settings entries and the icon editor's
 * controls from it, and presets and overrides are checked against it. Values for an icon come, each over the one
 * before: the settings (icon_*), the kind's preset (player models, or a car or rocket build type) and the item's or
 * module combination's own override, both stored in the database (ugc_icon_settings). Pure.
 */
namespace UgcIconParams {
	struct Param {
		std::string key;     // in presets and overrides, e.g. "sunStrength"
		std::string group;   // what it is part of, for the editor: camera, framing, model, sun, light, look
		std::string setting; // ugcconfig.ini, e.g. "icon_sun_strength"
		std::string label;
		std::string unit;
		float min{};
		float max{};
		float step{};
		float defaultValue{};
		std::string description;
		std::function<void(UgcRender::IconOptions&, float)> apply;
	};

	const std::vector<Param>& List();
	const Param* Find(std::string_view key);

	using Values = std::map<std::string, float>;

	// Known keys only, numbers only, clamped to their ranges
	Values Parse(std::string_view json);
	std::string ToJson(const Values& values);

	// Values over the options' own
	void Apply(UgcRender::IconOptions& options, const Values& values);

	// The options from the settings: `get` gives a setting's value (nullopt or unparsable: the parameter's default)
	UgcRender::IconOptions FromSettings(const std::function<std::optional<std::string>(const std::string&)>& get);

	// Where presets and overrides are stored (ugc_icon_settings.target)
	std::string KindTarget(const std::string& kind); // "kind:model", "kind:build6"
	std::string ModelTarget(int64_t id);             // "model:<id>"
	std::string CombinationTarget(const std::string& key); // "combo:<key>"
	// The kind of a player model, and of a car or rocket of build type `buildType` (ModuleComponent.buildType)
	std::string ModelKind();
	std::string BuildKind(int32_t buildType);
}
