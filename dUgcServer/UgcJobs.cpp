#include "UgcJobs.h"

#include <sstream>

#include "Sd0.h"
#include "UgcFormats.h"
#include "UgcModel.h"
#include "UgcModular.h"
#include "ZCompression.h"

namespace UgcJobs {
	void AddDownload(UgcStorage::Files& files, const std::string& name, const std::string& data) {
		files[name + ".gz"] = ZCompression::Gzip(data);
		files[name + ".checksum"] = UgcFormats::ChecksumXml(data);
	}

	std::string LxfmlFromBlob(const std::string& blob) {
		if (blob.starts_with("<?xml") || blob.starts_with("<LXFML")) return blob;
		std::stringstream stream(blob);
		try {
			Sd0 sd0(stream);
			return sd0.GetAsStringUncompressed();
		} catch (...) {
			return {};
		}
	}

	namespace {
		// The icon files of a model, and false when nothing was drawn
		bool AddIcon(UgcStorage::Files& files, const UgcModel::Model& model, const UgcRender::IconOptions& options) {
			const auto icon = UgcRender::RenderIcon(model, options);
			bool drawn = false;
			for (size_t i = 3; i < icon.rgba.size(); i += 4) drawn = drawn || icon.rgba[i] != 0;
			files["icon.png"] = UgcFormats::EncodePng(icon);
			AddDownload(files, "icon.dds", UgcFormats::EncodeDds(icon));
			return drawn;
		}
	}

	Outcome ProcessModel(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings) {
		Outcome outcome;
		const auto lxfml = LxfmlFromBlob(blob);
		if (lxfml.empty()) {
			outcome.error = "the stored LXFML can't be read";
			return outcome;
		}
		std::string error;
		const auto parts = UgcModel::ParseLxfml(lxfml, error);
		if (parts.empty()) {
			outcome.error = error;
			return outcome;
		}
		auto model = UgcModel::Build(parts, library);
		if (!model.missingDesigns.empty()) {
			outcome.note = "no geometry for design(s)";
			for (const auto design : model.missingDesigns) outcome.note += " " + std::to_string(design);
		}
		if (model.Empty()) {
			outcome.error = "none of the model's bricks have geometry";
			if (!outcome.note.empty()) outcome.error += " (" + outcome.note + ")";
			return outcome;
		}

		const auto optimized = UgcRender::Optimize(model, settings.optimize);
		outcome.aoBaked = settings.optimize.bakeAo;
		if (optimized.trianglesRemoved > 0) {
			if (!outcome.note.empty()) outcome.note += "; ";
			outcome.note += "removed " + std::to_string(optimized.trianglesRemoved) + " of " + std::to_string(optimized.trianglesBefore) + " triangles";
		}

		// Named like LU Toolbox names its shapes: S<shader>_<Opaque|Alpha>_<name>
		std::vector<std::pair<UgcModel::Mesh, bool>> pieces;
		for (const bool transparent : { false, true }) {
			for (auto& piece : UgcModel::Split(transparent ? model.transparent : model.opaque)) pieces.emplace_back(std::move(piece), transparent);
		}
		std::vector<UgcFormats::NifShape> shapes;
		size_t opaqueIndex = 0, transparentIndex = 0;
		for (const auto& [piece, transparent] : pieces) {
			const auto index = transparent ? transparentIndex++ : opaqueIndex++;
			shapes.push_back({ std::string("S01_") + (transparent ? "Alpha" : "Opaque") + "_Model" + (index > 0 ? std::to_string(index) : ""), &piece, transparent });
		}
		const auto nif = UgcFormats::WriteNif("SceneNode_Model", shapes);
		outcome.files["model.nif"] = nif;
		AddDownload(outcome.files, "model.nif", nif);
		AddDownload(outcome.files, "model.lxfml", lxfml);
		AddIcon(outcome.files, model, settings.icon);
		outcome.ok = true;
		return outcome;
	}

	Outcome ProcessModular(const ModularInput& input, const std::filesystem::path& res, const Settings& settings) {
		Outcome outcome;
		const auto build = UgcModular::ParseBuild(input.buildXml);
		if (!build) {
			outcome.error = "the build type has no topology in ModularBuildComponent";
			return outcome;
		}
		std::vector<UgcModular::Module> modules;
		for (const auto& moduleInput : input.modules) {
			const auto path = UgcBricks::ResolvePath(res, moduleInput.renderAsset);
			const auto data = path ? UgcBricks::ReadFile(*path) : std::nullopt;
			if (!data) {
				outcome.note += "module " + std::to_string(moduleInput.lot) + " has no mesh (" + moduleInput.renderAsset + "); ";
				continue;
			}
			std::string error;
			auto nif = NifFile::Parse(*data, 0, error);
			if (!nif) {
				outcome.note += "module " + std::to_string(moduleInput.lot) + ": " + error + "; ";
				continue;
			}
			modules.push_back({ moduleInput.partCode, std::move(*nif), UgcModular::ParseModuleConnections(moduleInput.moduleXml) });
		}
		if (modules.empty()) {
			outcome.error = "none of the modules have a mesh";
			if (!outcome.note.empty()) outcome.error += " (" + outcome.note + ")";
			return outcome;
		}
		const auto model = UgcModular::Assemble(*build, modules, outcome.note);
		if (model.Empty()) {
			outcome.error = "the modules have no triangles";
			return outcome;
		}
		auto options = settings.modularIcon;
		options.modelRotation = build->additionalRotation * options.modelRotation;
		AddIcon(outcome.files, model, options);
		outcome.ok = true;
		return outcome;
	}
}
