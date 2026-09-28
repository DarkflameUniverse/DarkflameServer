#include "UgcJobs.h"

#include <chrono>
#include <cmath>
#include <sstream>

#include "json.hpp"

#include "NifFile.h"
#include "Sd0.h"
#include "UgcFormats.h"
#include "UgcModel.h"
#include "UgcModular.h"
#include "ZCompression.h"

namespace UgcJobs {
	void AddDownload(UgcStorage::Files& files, const std::string& name, const std::string& data) {
		files[name + ".gz"] = ZCompression::Gzip(data);
		files[name + ".checksum"] = UgcFormats::ChecksumXml(data);
		// Without 3D services the client downloads it as sd0 (after asking its world for the checksum)
		auto sd0 = Sd0::Compress(data);
		if (!sd0.empty()) files[name + ".sd0"] = std::move(sd0);
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
		bool AddIcon(UgcStorage::Files& files, const UgcModel::Model& model, const UgcRender::IconOptions& options, const std::vector<float>* ao = nullptr) {
			const auto icon = UgcRender::RenderIcon(model, options, ao);
			bool drawn = false;
			for (size_t i = 3; i < icon.rgba.size(); i += 4) drawn = drawn || icon.rgba[i] != 0;
			files["icon.png"] = UgcFormats::EncodePng(icon);
			AddDownload(files, "icon.dds", UgcFormats::EncodeDds(icon));
			return drawn;
		}
	}

	size_t CountParts(std::string_view lxfml) {
		size_t count = 0;
		for (size_t at = lxfml.find("<Part"); at != std::string_view::npos; at = lxfml.find("<Part", at + 5)) count++;
		return count;
	}

	uint64_t EstimateMemory(size_t parts, const Settings& settings) {
		// Measured: the renders' buffers, and per brick its mesh in each LOD (positions, normals, colors, indices,
		// the occlusion tree and copies made along the way), about 40 KB at LOD 0
		const uint64_t resolution = static_cast<uint64_t>(std::clamp(settings.optimize.resolution, 64, 4096));
		const uint64_t icon = static_cast<uint64_t>(settings.icon.size) * settings.icon.supersample;
		const uint64_t fixed = resolution * resolution * 8 + icon * icon * 20 + 1024 * 1024 * 4 + 16 * 1024 * 1024;
		return fixed + static_cast<uint64_t>(parts) * 40 * 1024 * (1 + settings.lods.size());
	}

	namespace {
		double Since(std::chrono::steady_clock::time_point start) {
			return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		// An opaque mesh's pieces by look ([eLook]: UgcModel::Divide's pieces), the looks without a shader of their own
		// with the plastic ones
		using LookPieces = std::array<std::vector<UgcModel::Mesh>, UgcModel::LOOK_COUNT>;
		LookPieces DivideByLook(const UgcModel::Mesh& mesh, const std::array<bool, UgcModel::LOOK_COUNT>& separate) {
			LookPieces pieces;
			const auto split = UgcModel::SplitLooks(mesh, separate);
			if (!split) {
				pieces[0] = UgcModel::Divide(mesh);
				return pieces;
			}
			for (size_t look = 0; look < UgcModel::LOOK_COUNT; look++) pieces[look] = UgcModel::Divide((*split)[look]);
			return pieces;
		}
	}

	uint32_t Shaders::TagOf(UgcModel::eLook look) const {
		switch (look) {
			case UgcModel::eLook::METAL: return metal;
			case UgcModel::eLook::BRUSHED: return brushed;
			case UgcModel::eLook::GLOW: return glow;
			default: return 0;
		}
	}

	std::map<int32_t, UgcModel::eLook> Shaders::TagLooks() const {
		std::map<int32_t, UgcModel::eLook> looks{ { 88, UgcModel::eLook::METAL }, { 89, UgcModel::eLook::BRUSHED }, { 46, UgcModel::eLook::GLOW } };
		for (const auto look : { UgcModel::eLook::METAL, UgcModel::eLook::BRUSHED, UgcModel::eLook::GLOW }) {
			if (const auto tag = TagOf(look); tag != 0) looks[static_cast<int32_t>(tag)] = look;
		}
		return looks;
	}

	std::string ShapeName(const Settings& settings, UgcModel::eLook look, bool transparent) {
		if (transparent) return "S01_Alpha_Model";
		if (look == UgcModel::eLook::PLASTIC) return ("S" + settings.shaderOpaque + "_Opaque_Model").substr(0, 60);
		const auto tag = std::to_string(settings.shaders.TagOf(look));
		const char* name = look == UgcModel::eLook::METAL ? "_Metal_Model" : look == UgcModel::eLook::BRUSHED ? "_Brushed_Model" : "_Glow_Model";
		return ("S" + std::string(tag.size() < 2 ? "0" : "") + tag + name).substr(0, 60);
	}

	bool IconFromNif(const std::string& nif, const UgcRender::IconOptions& options, UgcStorage::Files& files, std::string& error,
		const std::map<int32_t, UgcModel::eLook>& tagLooks) {
		const auto readBack = NifFile::Parse(nif, 0, error);
		if (!readBack) return false;
		AddIcon(files, UgcModel::FromNif(*readBack, tagLooks), options);
		return true;
	}

	Outcome ProcessModel(const std::string& blob, UgcBricks::BrickLibrary& library, const Settings& settings, uint64_t seed, const UgcIconParams::Values& iconValues) {
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
			// A model with no bricks has nothing to make; the LXFML itself is still served
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
		const auto ranges = UgcModel::LodRanges(lods, settings.lodDistances);

		nlohmann::json stats;
		stats["version"] = 1;
		stats["bricks"] = parts.size();
		auto& lodStats = stats["lods"] = nlohmann::json::array();
		double buildMs = 0, hsrMs = 0, aoMs = 0;

		// Every LOD made like LU Toolbox makes each LOD collection: colored, hidden faces removed, lighting baked, divided
		std::vector<UgcModel::Model> models;
		std::vector<LookPieces> opaquePieces;
		std::vector<std::vector<UgcModel::Mesh>> transparentPieces;
		// The looks with a shader of their own (UgcJobs::Shaders), each an NiLODNode apart from the plastic
		std::array<bool, UgcModel::LOOK_COUNT> separate{};
		for (size_t look = 1; look < UgcModel::LOOK_COUNT; look++) separate[look] = settings.shaders.TagOf(static_cast<UgcModel::eLook>(look)) != 0;
		const bool glowApart = separate[static_cast<size_t>(UgcModel::eLook::GLOW)];
		UgcModel::Model preview; // LOD 0 before the lighting bake, for the dashboard
		for (size_t i = 0; i < lods.size(); i++) {
			auto options = settings.build;
			options.seed = seed;
			options.lod = lods[i];
			auto step = std::chrono::steady_clock::now();
			auto model = UgcModel::Build(parts, library, options);
			buildMs += Since(step);
			if (i == 0) {
				if (!model.missingDesigns.empty()) {
					outcome.note = "no geometry for design(s)";
					for (const auto design : model.missingDesigns) outcome.note += " " + std::to_string(design);
					stats["missingDesigns"] = model.missingDesigns;
				}
				if (model.Empty()) {
					outcome.error = "none of the model's bricks have geometry";
					if (!outcome.note.empty()) outcome.error += " (" + outcome.note + ")";
					return outcome;
				}
			}
			nlohmann::json entry{ { "lod", lods[i] }, { "near", ranges[i].first }, { "far", ranges[i].second },
				{ "opaqueBefore", model.opaque.TriangleCount() }, { "transparent", model.transparent.TriangleCount() } };
			step = std::chrono::steady_clock::now();
			const auto optimized = UgcRender::Optimize(model, settings.optimize);
			hsrMs += Since(step);
			if (i == 0 && optimized.trianglesRemoved > 0) {
				if (!outcome.note.empty()) outcome.note += "; ";
				outcome.note += "removed " + std::to_string(optimized.trianglesRemoved) + " of " + std::to_string(optimized.trianglesBefore) + " triangles";
			}
			if (i == 0) preview = model;
			// The emissive shader lerps from its own lighting to the vertex color, so glowing bricks keep their plain
			// color: no occlusion, and no glow added (it would glow twice)
			std::vector<glm::vec4> plainColors;
			if (glowApart && !model.opaque.looks.empty()) plainColors = model.opaque.colors;
			step = std::chrono::steady_clock::now();
			UgcRender::BakeAo(model, settings.ao);
			aoMs += Since(step);
			for (size_t v = 0; v < plainColors.size() && v < model.opaque.looks.size(); v++) {
				if (model.opaque.looks[v] == UgcModel::eLook::GLOW) model.opaque.colors[v] = glm::vec4(glm::vec3(plainColors[v]), 1.0f);
			}
			entry["opaqueAfter"] = model.opaque.TriangleCount();
			entry["vertices"] = model.opaque.positions.size() + model.transparent.positions.size();
			opaquePieces.push_back(DivideByLook(model.opaque, separate));
			transparentPieces.push_back(settings.combineTransparent ? UgcModel::Divide(model.transparent) : UgcModel::SplitAt(model.transparent, model.transparentBricks));
			size_t shapes = transparentPieces.back().size();
			for (const auto& pieces : opaquePieces.back()) shapes += pieces.size();
			entry["shapes"] = shapes;
			// Triangles per group (NiLODNode) when metal or glow have groups of their own
			if (std::find(separate.begin(), separate.end(), true) != separate.end()) {
				auto& byGroup = entry["groups"] = nlohmann::json::object();
				for (size_t look = 0; look < UgcModel::LOOK_COUNT; look++) {
					size_t triangles = 0;
					for (const auto& piece : opaquePieces.back()[look]) triangles += piece.TriangleCount();
					if (triangles > 0) byGroup[ShapeName(settings, static_cast<UgcModel::eLook>(look), false)] = triangles;
				}
				if (!model.transparent.Empty()) byGroup[ShapeName(settings, UgcModel::eLook::PLASTIC, true)] = model.transparent.TriangleCount();
			}
			lodStats.push_back(entry);
		}
		outcome.aoBaked = settings.ao.enabled;

		// An NiLODNode for the opaque bricks and one for the transparent ones, as LU Toolbox names them, and one for
		// each look with a shader of its own between them. Every group has every level (empty where it has nothing).
		const auto groups = [&](size_t levels, const std::vector<LookPieces>& opaque, const std::vector<std::vector<UgcModel::Mesh>>& transparent) {
			std::vector<UgcFormats::NifLodGroup> out;
			for (size_t kind = 0; kind <= UgcModel::LOOK_COUNT; kind++) {
				const bool isTransparent = kind == UgcModel::LOOK_COUNT;
				const auto look = isTransparent ? UgcModel::eLook::PLASTIC : static_cast<UgcModel::eLook>(kind);
				UgcFormats::NifLodGroup group{ ShapeName(settings, look, isTransparent), isTransparent, {} };
				if (look == UgcModel::eLook::GLOW) group.emissive = std::max(settings.shaders.glowEmissive, 0.0f);
				bool any = false;
				for (size_t i = 0; i < levels; i++) {
					UgcFormats::NifLod lod{ ranges[i].first, ranges[i].second, "LOD_" + std::to_string(lods[i]), {} };
					for (const auto& piece : (isTransparent ? transparent[i] : opaque[i][kind])) lod.pieces.push_back(&piece);
					any = any || !lod.pieces.empty();
					group.lods.push_back(std::move(lod));
				}
				if (any) out.push_back(std::move(group));
			}
			return out;
		};
		const auto nif = UgcFormats::WriteLodNif("SceneNode_Model", groups(lods.size(), opaquePieces, transparentPieces));
		// Stored compressed only (the client downloads .gz; the dashboard's copies are inflated when asked for). The
		// LXFML is served from the database.
		AddDownload(outcome.files, "model.nif", nif);
		{
			const std::vector<LookPieces> opaque{ DivideByLook(preview.opaque, separate) };
			const std::vector<std::vector<UgcModel::Mesh>> transparent{ transparentPieces[0] };
			outcome.files["model.noao.nif.gz"] = ZCompression::Gzip(UgcFormats::WriteLodNif("SceneNode_Model", groups(1, opaque, transparent)));
		}

		// The icon is drawn from the .nif just made (its most detailed LOD, read back like any client .nif), so it
		// shows what the game shows: the colors with their variation, hidden faces removed, the baked lighting.
		const auto iconStart = std::chrono::steady_clock::now();
		auto iconOptions = settings.icon;
		UgcIconParams::Apply(iconOptions, iconValues);
		std::string nifError;
		if (!IconFromNif(nif, iconOptions, outcome.files, nifError, settings.shaders.TagLooks())) {
			outcome.error = "the .nif made can't be read back for the icon: " + nifError;
			return outcome;
		}
		const double iconMs = Since(iconStart);

		stats["ms"] = { { "build", std::lround(buildMs) }, { "hiddenSurfaces", std::lround(hsrMs) }, { "ambientOcclusion", std::lround(aoMs) },
			{ "icon", std::lround(iconMs) }, { "total", std::lround(Since(started)) } };
		stats["settings"] = { { "palette", settings.build.palette == UgcModel::ePalette::LU_TOOLBOX ? "lu_toolbox" : "brickdb" },
			{ "colorVariation", settings.build.colorVariation }, { "transparentOpacity", settings.build.transparentOpacity },
			{ "removeHiddenFaces", settings.optimize.removeHidden }, { "groundPlane", settings.optimize.groundPlane },
			{ "ao", settings.ao.enabled }, { "aoDistance", settings.ao.distance }, { "aoSamples", settings.ao.samples }, { "aoStrength", settings.ao.strength } };
		outcome.stats = stats.dump();
		outcome.files["stats.json"] = outcome.stats;
		outcome.ok = true;
		return outcome;
	}

	std::optional<UgcModel::Model> AssembleModular(const ModularInput& input, const std::filesystem::path& res, glm::mat4& additionalRotation, std::string& error, std::string& note) {
		const auto build = UgcModular::ParseBuild(input.buildXml);
		if (!build) {
			error = "the build type has no topology in ModularBuildComponent";
			return std::nullopt;
		}
		additionalRotation = build->additionalRotation;
		std::vector<UgcModular::Module> modules;
		for (const auto& moduleInput : input.modules) {
			const auto path = UgcBricks::ResolvePath(res, moduleInput.renderAsset);
			const auto data = path ? UgcBricks::ReadFile(*path) : std::nullopt;
			if (!data) {
				note += "module " + std::to_string(moduleInput.lot) + " has no mesh (" + moduleInput.renderAsset + "); ";
				continue;
			}
			std::string nifError;
			auto nif = NifFile::Parse(*data, 0, nifError);
			if (!nif) {
				note += "module " + std::to_string(moduleInput.lot) + ": " + nifError + "; ";
				continue;
			}
			modules.push_back({ moduleInput.partCode, std::move(*nif), UgcModular::ParseModuleConnections(moduleInput.moduleXml) });
		}
		if (modules.empty()) {
			error = "none of the modules have a mesh";
			if (!note.empty()) error += " (" + note + ")";
			return std::nullopt;
		}
		auto model = UgcModular::Assemble(*build, modules, note);
		if (model.Empty()) {
			error = "the modules have no triangles";
			return std::nullopt;
		}
		return model;
	}

	Outcome ProcessModular(const ModularInput& input, const std::filesystem::path& res, const Settings& settings) {
		Outcome outcome;
		glm::mat4 additionalRotation{ 1.0f };
		const auto model = AssembleModular(input, res, additionalRotation, outcome.error, outcome.note);
		if (!model) return outcome;
		auto options = ModularIconOptions(input, settings);
		options.modelRotation = additionalRotation;
		AddIcon(outcome.files, *model, options);
		outcome.files["combo.json"] = nlohmann::json{ { "key", input.key }, { "buildType", input.buildType } }.dump();
		outcome.ok = true;
		return outcome;
	}

	std::optional<std::string> AssemblyNif(const ModularInput& input, const std::filesystem::path& res, std::string& error) {
		glm::mat4 additionalRotation{ 1.0f };
		std::string note;
		auto model = AssembleModular(input, res, additionalRotation, error, note);
		if (!model) return std::nullopt;
		// Turned as the icon renderer turns it before the pose's own rotation, so the editor's model rotation starts from here
		model->opaque.Transform(additionalRotation);
		model->transparent.Transform(additionalRotation);
		const auto opaque = UgcModel::Split(model->opaque), transparent = UgcModel::Split(model->transparent);
		std::vector<UgcFormats::NifShape> shapes;
		for (const auto& piece : opaque) shapes.push_back({ "S01_Opaque_Model", &piece, false });
		for (const auto& piece : transparent) shapes.push_back({ "S01_Alpha_Model", &piece, true });
		return UgcFormats::WriteNif("SceneNode_Assembly", shapes);
	}

	UgcRender::IconOptions ModularIconOptions(const ModularInput& input, const Settings& settings) {
		auto options = settings.icon;
		UgcIconParams::Apply(options, input.iconValues);
		return options;
	}


}
