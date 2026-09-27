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
	struct Settings {
		UgcRender::OptimizeOptions optimize;
		UgcRender::IconOptions icon;
		UgcRender::IconOptions modularIcon;
	};

	struct Outcome {
		bool ok{};
		std::string error;       // why it failed
		std::string note;        // what was odd but not fatal (missing bricks, ...)
		UgcStorage::Files files; // name -> bytes, when ok
		bool aoBaked{};
	};

	// The client's download of `data` (`name` + ".gz" and ".checksum") added to `files`
	void AddDownload(UgcStorage::Files& files, const std::string& name, const std::string& data);

	// The LXFML of a ugc row's lxfml column (an sd0 stream, or plain LXFML); empty when it can't be read
	std::string LxfmlFromBlob(const std::string& blob);

	// A player model: the optimized .nif, its icon and its LXFML for download
	Outcome ProcessModel(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings);

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
