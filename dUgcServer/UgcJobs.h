#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "UgcBricks.h"
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
	struct Settings {
		UgcModel::BuildOptions build;          // palette, color variation, transparent opacity
		std::vector<uint32_t> lods{ 0, 2 };    // brickprimitives levels made (LU Toolbox imports LOD 0 and 2; the client has no 3)
		UgcModel::LodDistances lodDistances;
		std::string shaderOpaque{ "01" };      // S<shader>_Opaque_...; transparent shapes are always S01
		bool combineTransparent{ false };      // one shape for all transparent bricks, else one per brick (Combine Transparent)
		UgcRender::OptimizeOptions optimize;   // hidden surface removal
		UgcRender::AoOptions ao;               // Bake Lighting (AO Only)
		UgcRender::IconOptions icon;           // player models: drawn from their .nif (LOD 0); its ao is not used
		UgcRender::IconOptions modularIcon;
		uint32_t maxBricks{};                  // a model with more fails; 0: no limit
	};

	struct Outcome {
		bool ok{};
		std::string error;       // why it failed
		std::string note;        // what was odd but not fatal (missing bricks, ...)
		UgcStorage::Files files; // name -> bytes, when ok
		bool aoBaked{};
		std::string stats;       // stats.json: bricks, triangles before and after per LOD, timings
	};

	// The client's download of `data` (`name` + ".gz" and ".checksum") added to `files`
	void AddDownload(UgcStorage::Files& files, const std::string& name, const std::string& data);

	// The LXFML of a ugc row's lxfml column (an sd0 stream, or plain LXFML); empty when it can't be read
	std::string LxfmlFromBlob(const std::string& blob);

	// A player model: the optimized .nif, its icon and its LXFML for download, and stats.json. `seed` picks the color
	// variation's random numbers (the model's id, so making it again gives the same colors).
	Outcome ProcessModel(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings, uint64_t seed = 0);

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
		std::string buildXml; // ModularBuildComponent.xml of the modules' build type
	};

	// A modular build's icon, from its modules' meshes put together
	Outcome ProcessModular(const ModularInput& input, const std::filesystem::path& res, const Settings& settings);
}
