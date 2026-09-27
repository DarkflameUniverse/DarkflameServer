#include "UgcIconParams.h"

#include <algorithm>
#include <cmath>

#include "GeneralUtils.h"
#include "json.hpp"

namespace {
	using Options = UgcRender::IconOptions;
}

namespace UgcIconParams {
	const std::vector<Param>& List() {
		// Framing from LU Toolbox's icon renderer (its UGC render add-on's BrickBuild scene); the light matched to the
		// brightness of the game's own model icons (res/textures/ui/inventory/models), docs/UgcServer.md
		static const std::vector<Param> list = {
			{ "yaw", "icon_yaw", "Angle around", "degrees", -180, 180, 1, 53.36f, "The camera's angle around the model, from its front.", [](Options& o, float v) { o.yawDegrees = v; } },
			{ "pitch", "icon_pitch", "Angle above", "degrees", -89, 89, 1, 19.54f, "", [](Options& o, float v) { o.pitchDegrees = v; } },
			{ "fov", "icon_fov", "Field of view", "degrees", 5, 90, 0.5f, 39.6f, "", [](Options& o, float v) { o.fovDegrees = v; } },
			{ "margin", "icon_margin", "Border (zoom)", "", 0.5f, 3, 0.01f, 1.03f, "1 fills the icon, more leaves a border.", [](Options& o, float v) { o.margin = v; } },
			{ "offsetX", "icon_offset_x", "Shift right", "", -0.5f, 0.5f, 0.01f, 0.0f, "Share of the icon's width.", [](Options& o, float v) { o.offsetX = v; } },
			{ "offsetY", "icon_offset_y", "Shift up", "", -0.5f, 0.5f, 0.01f, 0.0f, "Share of the icon's height.", [](Options& o, float v) { o.offsetY = v; } },
			{ "sunYaw", "icon_sun_yaw", "Sun around", "degrees", -180, 180, 1, 21.0f, "", [](Options& o, float v) { o.sunYawDegrees = v; } },
			{ "sunPitch", "icon_sun_pitch", "Sun above", "degrees", -10, 90, 1, 50.3f, "", [](Options& o, float v) { o.sunPitchDegrees = v; } },
			{ "sunStrength", "icon_sun_light", "Sun strength", "", 0, 10, 0.05f, 2.0f, "", [](Options& o, float v) { o.sunStrength = v; } },
			{ "ambient", "icon_world_light", "World light", "", 0, 2, 0.01f, 1.0f, "What every face gets.", [](Options& o, float v) { o.ambient = v; } },
			{ "fill", "icon_fill", "Fill light", "", 0, 5, 0.05f, 0.8f, "A light from the camera.", [](Options& o, float v) { o.fill = v; } },
			{ "specular", "icon_specular", "Highlights", "", 0, 2, 0.05f, 0.2f, "The sun's highlight on the plastic.", [](Options& o, float v) { o.specular = v; } },
			{ "shininess", "icon_shininess", "Highlight tightness", "", 1, 200, 1, 40.0f, "", [](Options& o, float v) { o.shininess = v; } },
			{ "exposure", "icon_exposure", "Exposure", "", 0.1f, 4, 0.05f, 2.6f, "Brightness of everything.", [](Options& o, float v) { o.exposure = v; } },
			{ "contrast", "icon_contrast", "Contrast", "", 0.5f, 1.5f, 0.01f, 1.0f, "", [](Options& o, float v) { o.contrast = v; } },
			{ "shadows", "icon_shadow_strength", "Shadows", "", 0, 1, 0.05f, 0.4f, "How much the sun's shadows darken.", [](Options& o, float v) { o.shadows = v; } },
			{ "aoStrength", "icon_ao_strength", "Ambient occlusion", "", 0, 1, 0.05f, 0.0f,
				"Occlusion worked out for the icon. Player models' meshes have theirs baked in already.", [](Options& o, float v) { o.ao.strength = v; o.ao.enabled = v > 0.0f; } },
		};
		return list;
	}

	const Param* Find(std::string_view key) {
		const auto& list = List();
		const auto it = std::find_if(list.begin(), list.end(), [key](const Param& p) { return p.key == key; });
		return it != list.end() ? &*it : nullptr;
	}

	Values Parse(std::string_view text) {
		Values values;
		const auto json = nlohmann::json::parse(text, nullptr, false);
		if (!json.is_object()) return values;
		for (const auto& param : List()) {
			const auto it = json.find(param.key);
			if (it == json.end() || !it->is_number()) continue;
			const auto value = it->get<float>();
			if (std::isfinite(value)) values[param.key] = std::clamp(value, param.min, param.max);
		}
		return values;
	}

	std::string ToJson(const Values& values) {
		nlohmann::json json = nlohmann::json::object();
		for (const auto& [key, value] : values) {
			if (Find(key)) json[key] = value;
		}
		return json.dump();
	}

	void Apply(UgcRender::IconOptions& options, const Values& values) {
		for (const auto& [key, value] : values) {
			if (const auto* param = Find(key)) param->apply(options, std::clamp(value, param->min, param->max));
		}
	}

	UgcRender::IconOptions FromSettings(const std::function<std::optional<std::string>(const std::string&)>& get) {
		UgcRender::IconOptions options;
		for (const auto& param : List()) {
			const auto text = get(param.setting);
			const auto value = text ? GeneralUtils::TryParse<float>(*text) : std::nullopt;
			param.apply(options, std::clamp(value.value_or(param.defaultValue), param.min, param.max));
		}
		return options;
	}

	std::string KindTarget(const std::string& kind) { return "kind:" + kind; }
	std::string ModelTarget(int64_t id) { return "model:" + std::to_string(id); }
	std::string CombinationTarget(const std::string& key) { return "combo:" + key; }
	std::string ModelKind() { return "model"; }
	std::string BuildKind(int32_t buildType) { return "build" + std::to_string(buildType); }
}
