#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "UgcBricks.h"
#include "UgcHsr.h"
#include "UgcIconParams.h"
#include "UgcRender.h"
#include "UgcStorage.h"

/**
 * The work a UGC worker thread does for one item: everything from the stored LXFML (or a modular build's modules) to
 * the finished files. No database, network or CDClient: the main thread gathers the input and stores the result.
 */
namespace UgcJobs {
	/**
	 * How models are made. The defaults are LU Toolbox's (Process Model, Bake Lighting and the icon renderer), see the
	 * parity table in docs/UgcServer.md.
	 */
	/**
	 * The shaders of the metal, glow and glitter colors (UgcModel::eLook): the mapShaders id each look's own NiLODNode
	 * names (S88_Metal_Model, ...), 0 for none (the colors stay in S01_Opaque_Model, or S01_Alpha_Model when
	 * transparent, as on live). Not what live did: live's models are all S01 (docs/UgcServer.md, "Metal and glow").
	 */
	struct Shaders {
		uint32_t metal{};        // shader_metal: 88 Polished Metal
		uint32_t brushed{};      // shader_brushed: 89 Brushed Steel
		uint32_t glow{};         // shader_glow: 46 LEGO-Emissive
		uint32_t glitter{};      // shader_glitter: 21 LEGO-AnimUV (opaque and transparent glitter, each a group)
		float glowEmissive{ 1.0f }; // glow_emissive: the glow shapes' NiMaterialProperty emissive (how much the vertex color shows unlit)
		UgcGlitter::Params glitterParams; // glitter_size, glitter_density, glitter_speed

		// The mapShaders id of a look's group, 0 for the plastic S01_Opaque_Model
		uint32_t TagOf(UgcModel::eLook look) const;
		// Multishader tag -> look, for reading the looks back out of a .nif (the icon): these settings' ids, and the
		// client's Polished Metal (88), Brushed Steel (89), LEGO-Emissive (46) and LEGO-AnimUV (21) for .nifs made with
		// other settings
		std::map<int32_t, UgcModel::eLook> TagLooks() const;
	};

	struct Settings {
		UgcModel::BuildOptions build;          // palette, color variation, transparent opacity
		std::vector<uint32_t> lods{ 0, 2 };    // brickprimitives levels made (LU Toolbox imports LOD 0 and 2; the client has no 3)
		UgcModel::LodDistances lodDistances;
		std::string shaderOpaque{ "01" };      // S<shader>_Opaque_...; transparent shapes are always S01
		bool combineTransparent{ false };      // one shape for all transparent bricks, else one per brick (Combine Transparent)
		Shaders shaders;                       // metal and glow groups (all off by default)
		UgcHsr::Options hsr;                   // hidden surface removal (Remove Hidden Faces)
		UgcRender::AoOptions ao;               // Bake Lighting (AO Only)
		UgcRender::IconOptions icon;           // from the icon_* settings (UgcIconParams); presets and overrides go over it
		uint32_t maxBricks{};                  // a model with more fails; 0: no limit
	};

	struct Outcome {
		bool ok{};
		std::string error;       // why it failed
		std::string note;        // what was odd but not fatal (missing bricks, ...)
		UgcStorage::Files files; // name -> bytes, when ok
		bool aoBaked{};
		bool empty{};            // nothing to make (no bricks): not a failure
		std::string stats;       // stats.json: bricks, triangles before and after per LOD, timings
	};

	// The client's download of `data` (`name` + ".gz" and ".checksum") added to `files`
	void AddDownload(UgcStorage::Files& files, const std::string& name, const std::string& data);

	// The LXFML of a ugc row's lxfml column (an sd0 stream, or plain LXFML); empty when it can't be read
	std::string LxfmlFromBlob(const std::string& blob);

	// A player model: the optimized .nif, its icon and its LXFML for download, and stats.json. `seed` picks the color
	// variation's random numbers (the model's id, so making it again gives the same colors).
	// `iconValues`: the player models' preset and this model's override (UgcIconParams), over settings.icon.
	Outcome ProcessModel(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings, uint64_t seed = 0,
		const UgcIconParams::Values& iconValues = {});

	// A model's icon files (icon.png, icon.dds download) drawn from its .nif (LOD 0), its metal and glow groups by
	// `tagLooks` (Shaders::TagLooks); false (and `error`) when the .nif can't be read
	bool IconFromNif(const std::string& nif, const UgcRender::IconOptions& options, UgcStorage::Files& files, std::string& error,
		const std::map<int32_t, UgcModel::eLook>& tagLooks = {});

	// The name of a group of shapes (its NiLODNode and shapes): S01_Opaque_Model, S01_Alpha_Model, S88_Metal_Model,
	// S89_Brushed_Model, S46_Glow_Model, S21_Glitter_Model and S21_GlitterAlpha_Model (transparent glitter; the ids from
	// the settings), at most 60 characters as LU Toolbox cuts them
	std::string ShapeName(const Settings& settings, UgcModel::eLook look, bool transparent);

	// How many bricks (parts) an LXFML has, counted cheaply (for the memory estimate before a job starts)
	size_t CountParts(std::string_view lxfml);

	// About how much memory making a model of `parts` bricks takes, in bytes
	uint64_t EstimateMemory(size_t parts, const Settings& settings);

	struct ModuleInput {
		uint32_t lot{};
		uint32_t partCode{};
		std::string renderAsset; // RenderComponent.render_asset, relative to the client's res folder
		std::string moduleXml;   // ModuleComponent.xml
	};

	struct ModularInput {
		std::vector<ModuleInput> modules;
		std::string buildXml;  // ModularBuildComponent.xml of the modules' build type
		int32_t buildType{};   // 3 rocket, 6 car
		std::string key;       // the combination (UgcModularKey::Normalize)
		UgcIconParams::Values iconValues; // the build type's preset and the combination's override (DB), over the settings
	};

	// The icon options for a car or rocket: the settings, then its build type's preset and the combination's override
	UgcRender::IconOptions ModularIconOptions(const ModularInput& input, const Settings& settings);

	// A modular build's modules' meshes put together (UgcModular::Assemble), not yet turned by the build type's
	// AdditionalModelRotation (given back in `additionalRotation`); nullopt with `error` when there's nothing to draw
	std::optional<UgcModel::Model> AssembleModular(const ModularInput& input, const std::filesystem::path& res, glm::mat4& additionalRotation, std::string& error, std::string& note);

	// A modular build's icon (icon.png, icon.dds download, combo.json), from its modules' meshes put together
	Outcome ProcessModular(const ModularInput& input, const std::filesystem::path& res, const Settings& settings);

	// The assembled mesh as the icon renderer turns it (AdditionalModelRotation applied), as a .nif for the dashboard's
	// pose editor; nullopt with `error` when there's nothing to draw
	std::optional<std::string> AssemblyNif(const ModularInput& input, const std::filesystem::path& res, std::string& error);
}
