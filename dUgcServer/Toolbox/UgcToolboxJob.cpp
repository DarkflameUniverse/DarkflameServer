// UgcJobs::ProcessModelToolbox: a player model made by LU Toolbox in Blender (UgcToolbox), checked and turned into the
// files a native make writes

#include <algorithm>
#include <chrono>
#include <cmath>

#include "json.hpp"

#include "NifFile.h"
#include "UgcJobs.h"
#include "UgcModel.h"
#include "UgcThrottle.h"
#include "UgcToolbox.h"

namespace UgcJobs {
	namespace {
		double Since(std::chrono::steady_clock::time_point start) {
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		size_t Triangles(const NifFile::Model& model) {
			size_t triangles = 0;
			for (const auto& mesh : model.meshes) triangles += mesh.indices.size() / 3;
			return triangles;
		}
	}

	Outcome ProcessModelToolbox(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings, UgcToolbox::Worker& toolbox,
		uint64_t id, const UgcIconParams::Values& iconValues) {
		Outcome outcome;
		const auto started = std::chrono::steady_clock::now();
		const auto lxfml = LxfmlFromBlob(blob);
		if (lxfml.empty()) {
			outcome.error = "the stored LXFML can't be read";
			return outcome;
		}
		std::string error;
		const auto parts = UgcModel::ParseLxfml(lxfml, error);
		if (parts.empty()) {
			outcome.error = error;
			outcome.empty = UgcModel::HasNoBricks(lxfml);
			return outcome;
		}
		if (settings.maxBricks > 0 && parts.size() > settings.maxBricks) {
			outcome.error = "the model has " + std::to_string(parts.size()) + " bricks, more than max_model_bricks (" + std::to_string(settings.maxBricks) + ")";
			return outcome;
		}
		auto lods = settings.lods;
		std::erase_if(lods, [](uint32_t lod) { return lod > 3; });
		std::sort(lods.begin(), lods.end());
		lods.erase(std::unique(lods.begin(), lods.end()), lods.end());
		if (lods.empty()) lods.push_back(0);

		// The triangles before: the bricks' own meshes, as LU Toolbox's importer reads them from the same files
		nlohmann::json stats;
		stats["version"] = 1;
		stats["bricks"] = parts.size();
		auto& lodStats = stats["lods"] = nlohmann::json::array();
		auto step = std::chrono::steady_clock::now();
		for (size_t i = 0; i < lods.size(); i++) {
			auto options = settings.build;
			options.seed = id;
			options.lod = lods[i];
			const auto model = UgcModel::Build(parts, library, options);
			if (i == 0 && !model.missingDesigns.empty()) {
				outcome.note = "no geometry for design(s)";
				for (const auto design : model.missingDesigns) outcome.note += " " + std::to_string(design);
				stats["missingDesigns"] = model.missingDesigns;
			}
			if (i == 0 && model.Empty()) {
				outcome.error = "none of the model's bricks have geometry";
				if (!outcome.note.empty()) outcome.error += " (" + outcome.note + ")";
				return outcome;
			}
			lodStats.push_back({ { "lod", lods[i] }, { "opaqueBefore", model.opaque.TriangleCount() }, { "transparent", model.transparent.TriangleCount() } });
		}
		const double countMs = Since(step);

		// LU Toolbox makes the .nif. Blender's CPU time is this thread's (the budget, the make's CPU time), and the
		// budget is kept by pausing Blender.
		step = std::chrono::steady_clock::now();
		const auto made = toolbox.Make(id, lxfml, lods, [](double cpuSeconds, const UgcToolbox::Worker::Pause& pause) {
			UgcThrottle::Charge(cpuSeconds);
			UgcThrottle::Checkpoint(pause);
		});
		const double toolboxMs = Since(step);
		if (!made.ok) {
			outcome.error = made.error;
			return outcome;
		}

		// Read back as the icon renderer and the dashboard read it: every level, for the triangles after
		for (size_t i = 0; i < lods.size(); i++) {
			std::string nifError;
			const auto read = NifFile::Parse(made.nif, static_cast<uint32_t>(i), nifError);
			if (!read) {
				outcome.error = "LU Toolbox's .nif can't be read: " + nifError;
				return outcome;
			}
			auto& entry = lodStats[i];
			const auto triangles = Triangles(*read);
			const auto transparent = entry.value("transparent", size_t{ 0 });
			entry["opaqueAfter"] = triangles > transparent ? triangles - transparent : 0;
			entry["trianglesInNif"] = triangles;
			entry["shapes"] = read->meshes.size();
			size_t vertices = 0;
			for (const auto& mesh : read->meshes) vertices += mesh.positions.size() / 3;
			entry["vertices"] = vertices;
			if (i == 0 && read->meshes.empty()) {
				outcome.error = "LU Toolbox's .nif has no meshes";
				return outcome;
			}
		}
		AddDownload(outcome.files, "model.nif", made.nif);

		// The icon from the .nif, as a native model's (its baked light as it is: no denoising without a .nif before the bake)
		const auto iconStart = std::chrono::steady_clock::now();
		auto iconOptions = settings.icon;
		UgcIconParams::Apply(iconOptions, iconValues);
		iconOptions.denoise = UgcRender::eDenoise::OFF;
		std::string nifError;
		if (!IconFromNif(made.nif, iconOptions, outcome.files, nifError, settings.shaders.TagLooks(), settings.shaders.OverlayTags())) {
			outcome.error = "LU Toolbox's .nif can't be read back for the icon: " + nifError;
			return outcome;
		}
		const double iconMs = Since(iconStart);

		// The steps as the native ones are named, so the comparison lines up: Process Model (hidden faces removed, and
		// LU Toolbox's other preparations) as hidden surfaces, Bake Lighting as ambient occlusion
		const auto part = [&made](const char* name) { return std::lround(made.ms.value(name, 0.0)); };
		stats["ms"] = { { "build", part("import") }, { "hiddenSurfaces", part("process") }, { "ambientOcclusion", part("bake") }, { "export", part("export") },
			{ "reset", part("reset") }, { "icon", std::lround(iconMs) }, { "count", std::lround(countMs) }, { "toolbox", std::lround(toolboxMs) },
			{ "waited", std::lround(made.waitedMs) }, { "blenderCpu", std::lround(made.blenderCpuSeconds * 1000.0) }, { "total", std::lround(Since(started)) } };
		const auto& config = toolbox.GetConfig();
		stats["settings"] = { { "processor", std::string(UgcProcessOptions::TOOLBOX_BLENDER) }, { "toolbox", made.blender }, { "device", config.device },
			{ "threads", config.threads } };
		if (made.started) stats["blenderStarted"] = true;
		outcome.options = std::string(UgcProcessOptions::TOOLBOX_BLENDER);
		outcome.aoBaked = true;
		outcome.stats = stats.dump();
		outcome.files["stats.json"] = outcome.stats;
		outcome.ok = true;
		return outcome;
	}
}
