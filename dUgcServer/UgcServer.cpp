// The UGC server: makes the meshes and icons of what players build and serves them, and the models' LXFML, to the
// game client over HTTP. See docs/UgcServer.md.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <set>
#include <thread>

#include "Profiler.h"
#include "AssetManager.h"
#include "BinaryPathFinder.h"
#include "FdbSnapshot.h"
#include "CDClientDatabase.h"
#include "ConfigSync.h"
#include "Database.h"
#include "Diagnostics.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MasterPackets.h"
#include "Server.h"
#include "ServiceType.h"
#include "Web.h"
#include "dConfig.h"
#include "dServer.h"
#include "TrafficStats.h"
#include "eHTTPMethod.h"
#include "json.hpp"

#include "UgcBricks.h"
#include "UgcCdClient.h"
#include "UgcIconParams.h"
#include "UgcJobs.h"
#include "Sd0.h"
#include "UgcFormats.h"
#include "ZCompression.h"
#include "UgcModel.h"
#include "UgcProcessor.h"
#include "UgcStorage.h"
#include "UgcThrottle.h"
#include "BuildInfo.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

namespace {
	dServer* g_Server = nullptr;
	std::string g_AdminKey; // the master password: the dashboard's admin requests carry it (X-Ugc-Admin-Key)
	UgcProcessor* g_Processor = nullptr;
	UgcStorage* g_Storage = nullptr;

	template<typename T>
	T Setting(const std::string& key, T fallback) {
		return GeneralUtils::TryParse<T>(Game::config->GetValue(key)).value_or(fallback);
	}

	// A list of color ids setting: empty is `fallback`, "none" (or anything without ids) is no colors
	std::set<uint32_t> ColorList(const std::string& key, const std::string& fallback) {
		const auto value = Game::config->GetValue(key);
		std::set<uint32_t> colors;
		std::stringstream stream(value.empty() ? fallback : value);
		std::string id;
		while (std::getline(stream, id, ',')) {
			std::erase_if(id, [](unsigned char c) { return std::isspace(c); });
			if (const auto color = GeneralUtils::TryParse<uint32_t>(id)) colors.insert(*color);
		}
		return colors;
	}

	std::vector<uint32_t> ParseLods(const std::string& text) {
		std::vector<uint32_t> lods;
		std::stringstream stream(text);
		std::string item;
		while (std::getline(stream, item, ',')) {
			std::erase_if(item, [](unsigned char c) { return std::isspace(c); });
			if (const auto lod = GeneralUtils::TryParse<uint32_t>(item); lod && *lod <= 3) lods.push_back(*lod);
		}
		return lods;
	}

	// ugcconfig.ini (and the dashboard's settings); the defaults are LU Toolbox's
	UgcJobs::Settings ReadSettings() {
		UgcJobs::Settings settings;
		settings.build.palette = Game::config->GetValue("color_palette") == "brickdb" ? UgcModel::ePalette::BRICKDB : UgcModel::ePalette::LU_TOOLBOX;
		settings.build.colorVariation = std::clamp(Setting<float>("color_variation", 5.0f), 0.0f, 100.0f);
		settings.build.transparentOpacity = std::clamp(Setting<float>("transparent_opacity", 58.82f), 0.0f, 100.0f);
		settings.build.brightness = std::clamp(Setting<float>("color_brightness", 100.0f), 0.0f, 200.0f);
		{
			// Empty: the default (129); none: no colors
			const auto value = Game::config->GetValue("transparent_colors");
			std::stringstream stream(value.empty() ? "129" : value);
			std::string id;
			while (std::getline(stream, id, ',')) {
				std::erase_if(id, [](unsigned char c) { return std::isspace(c); });
				if (const auto color = GeneralUtils::TryParse<uint32_t>(id)) settings.build.transparentColors.insert(*color);
			}
		}
		const auto lods = ParseLods(Game::config->GetValue("lods").empty() ? "0,2" : Game::config->GetValue("lods"));
		if (!lods.empty()) settings.lods = lods;
		settings.lodDistances.lod0 = Setting<float>("lod_distance_0", 0.0f);
		settings.lodDistances.lod1 = Setting<float>("lod_distance_1", 50.0f);
		settings.lodDistances.lod2 = Setting<float>("lod_distance_2", 100.0f);
		settings.lodDistances.lod3 = Setting<float>("lod_distance_3", 280.0f);
		settings.lodDistances.cull = Setting<float>("lod_cull", 10000.0f);
		if (!Game::config->GetValue("shader_opaque").empty()) settings.shaderOpaque = Game::config->GetValue("shader_opaque");
		settings.combineTransparent = Setting<int32_t>("combine_transparent", 0) != 0;
		// Metal and glow colors in NiLODNodes of their own, drawn with those shaders (off by default: not how live looked)
		settings.shaders.metal = std::min(Setting<uint32_t>("shader_metal", 88), 9999u);
		settings.shaders.brushed = std::min(Setting<uint32_t>("shader_brushed", 89), 9999u);
		settings.shaders.glow = std::min(Setting<uint32_t>("shader_glow", 46), 9999u);
		settings.shaders.glowEmissive = std::clamp(Setting<float>("glow_emissive", 1.0f), 0.0f, 10.0f);
		// Glitter colors in S<id>_Glitter_Model and S<id>_GlitterAlpha_Model with flecks (LEGO-AnimUV), and sparkles
		// over them in S<id>_GlitterSparkle_Model (Distortion Directional, which moves on its own)
		settings.shaders.glitter = std::min(Setting<uint32_t>("shader_glitter", 21), 9999u);
		settings.shaders.sparkle = std::min(Setting<uint32_t>("shader_glitter_sparkle", 79), 9999u);
		settings.shaders.glitterParams.tile = std::clamp(Setting<float>("glitter_size", 1.6f), 0.1f, 100.0f);
		settings.shaders.glitterParams.flecks = std::min(Setting<uint32_t>("glitter_density", 80), 5000u);
		settings.shaders.glitterParams.fleckSize = std::clamp(Setting<float>("glitter_fleck_size", 0.05f), 0.005f, 1.0f);
		settings.shaders.glitterParams.fleckOpacity = std::clamp(Setting<float>("glitter_fleck_opacity", 80.0f), 0.0f, 100.0f);
		settings.shaders.glitterParams.sparkleSize = std::clamp(Setting<float>("glitter_sparkle_size", 0.1f), 0.01f, 1.0f);
		settings.shaders.glitterParams.sparkleAmount = std::clamp(Setting<float>("glitter_sparkle_amount", 5.0f), 0.0f, 50.0f);
		settings.shaders.glitterParams.speed = std::clamp(Setting<float>("glitter_speed", 1.0f), 0.1f, 4.0f);
		settings.shaders.glitterParams.sparkleTint = std::clamp(Setting<float>("glitter_sparkle_tint", 30.0f), 0.0f, 100.0f);
		settings.shaders.glitterParams.sparkleBrightness = std::clamp(Setting<float>("glitter_sparkle_brightness", 100.0f), 0.0f, 100.0f);
		settings.shaders.glitterParams.random = Setting<int32_t>("glitter_random", 1) != 0;
		// Which Materials.xml MaterialTypes are metal, brushed steel and glitter
		for (const auto& [key, look] : { std::pair{ "metal_material_types", UgcModel::eLook::METAL }, std::pair{ "brushed_material_types", UgcModel::eLook::BRUSHED },
			std::pair{ "glitter_material_types", UgcModel::eLook::GLITTER } }) {
			const auto value = Game::config->GetValue(key);
			if (value.empty()) continue;
			std::erase_if(settings.build.looks.materialTypes, [look](const auto& entry) { return entry.second == look; });
			std::stringstream stream(value);
			std::string type;
			while (std::getline(stream, type, ',')) {
				std::erase_if(type, [](unsigned char c) { return std::isspace(c); });
				if (!type.empty() && type != "none") settings.build.looks.materialTypes[type] = look;
			}
		}
		// LEGO color ids drawn as brushed steel (by default the drum lacquered colors) and as glitter (by default the
		// ones LEGO's own color data files as glitter that the client's Materials.xml calls shinyPlastic) whatever their
		// Materials.xml type; empty: the default, none: no colors
		for (const auto color : ColorList("brushed_colors", "298,300,1002,1004")) settings.build.looks.colors[color] = UgcModel::eLook::BRUSHED;
		for (const auto color : ColorList("glitter_colors", "114,117")) settings.build.looks.colors[color] = UgcModel::eLook::GLITTER;
		// Satin (opal) colors: milky and less see-through in the transparent group (the client has no satin shader)
		settings.build.satinColors = ColorList("satin_colors", "360,362,363,364,365,366,367,376");
		settings.build.satinOpacity = std::clamp(Setting<float>("satin_opacity", 75.0f), 0.0f, 100.0f);
		settings.build.satinWhiten = std::clamp(Setting<float>("satin_whiten", 20.0f), 0.0f, 100.0f);
		settings.hsr.enabled = Setting<int32_t>("remove_hidden_faces", 1) != 0;
		settings.hsr.groundPlane = Setting<int32_t>("hsr_ground_plane", 0) != 0;
		settings.hsr.resolution = std::clamp(Setting<int32_t>("hsr_resolution", 1024), 64, 4096);
		// What traces the occlusion rays (the icon's too)
		settings.ao.rays = UgcRays::Parse(Game::config->GetValue("ray_backend")).value_or(UgcRays::eBackend::EMBREE);
		// The GPU hiprt uses (read before it is first used; changing it takes a restart)
		UgcRays::SetGpuDevice(UgcRays::eBackend::HIPRT, std::max(Setting<int32_t>("hiprt_device", 0), 0));
		UgcRays::SetGpuDevice(UgcRays::eBackend::EMBREE_GPU, std::max(Setting<int32_t>("embree_gpu_device", 0), 0));
		settings.ao.enabled = Setting<int32_t>("bake_ao", 1) != 0;
		settings.ao.distance = Setting<float>("ao_distance", 5.0f);
		settings.ao.samples = std::clamp(Setting<int32_t>("ao_samples", 64), 1, 1024);
		settings.ao.strength = Setting<float>("ao_strength", 1.0f);
		settings.ao.glowStrength = Setting<float>("glow_strength", 6.0f);
		// The icon's framing and light: the icon_* settings, listed with their defaults in UgcIconParams
		settings.icon = UgcIconParams::FromSettings([](const std::string& key) -> std::optional<std::string> {
			const auto value = Game::config->GetValue(key);
			return value.empty() ? std::nullopt : std::optional(value);
		});
		settings.icon.size = Setting<int32_t>("icon_size", 128);
		settings.icon.glowEmissive = settings.shaders.glowEmissive;
		settings.icon.glitter = settings.shaders.glitterParams;
		settings.icon.ao.distance = settings.ao.distance;
		settings.icon.ao.rays = settings.ao.rays;
		settings.icon.denoise = UgcRender::ParseDenoise(Game::config->GetValue("denoise")).value_or(UgcRender::eDenoise::OFF);
		settings.icon.denoiseSamples = std::clamp(Setting<int32_t>("denoise_samples", 4), 1, 256);
		settings.icon.bakedAo = settings.ao.enabled ? std::clamp(settings.ao.strength, 0.0f, 1.0f) : 0.0f;
		settings.maxBricks = Setting<uint32_t>("max_model_bricks", 0);
		// What makes the models: native, or toolbox-blender (LU Toolbox in Blender, when it can run: ReadToolboxConfig)
		const auto processorName = Game::config->GetValue("processor");
		if (UgcProcessOptions::Contains(UgcProcessOptions::PROCESSOR, processorName)) settings.processor = processorName;
		return settings;
	}

	// A path setting: relative ones are next to the server binaries; empty is `fallback` (empty: none)
	std::filesystem::path PathSetting(const std::string& key, const std::string& fallback = "") {
		const auto value = Game::config->GetValue(key).empty() ? fallback : Game::config->GetValue(key);
		if (value.empty()) return {};
		std::filesystem::path path(value);
		return path.is_relative() ? BinaryPathFinder::GetBinaryDir() / path : path;
	}

	std::filesystem::path ResPath();

	// LU Toolbox in a headless Blender (processor=toolbox-blender, docs/UgcServer.md "LU Toolbox in Blender"): the
	// external programs and folders it needs
	UgcToolbox::Config ReadToolboxConfig() {
		UgcToolbox::Config config;
		config.blender = PathSetting("toolbox_blender");
		config.standalone = PathSetting("toolbox_standalone_dir");
		config.scripts = PathSetting("toolbox_scripts_dir");
		config.worker = BinaryPathFinder::GetBinaryDir() / "ugc-toolbox" / "dlu_toolbox_worker.py";
		config.brickdb = PathSetting("toolbox_brickdb_dir", "toolbox-brickdb");
		config.work = PathSetting("toolbox_work_dir", "toolbox-work");
		config.res = ResPath();
		if (!Game::config->GetValue("toolbox_device").empty()) config.device = Game::config->GetValue("toolbox_device");
		config.threads = std::clamp(Setting<uint32_t>("toolbox_threads", 4), 1u, 256u);
		config.timeoutSeconds = std::clamp(Setting<uint32_t>("toolbox_timeout_seconds", 1800), 10u, 86400u);
		config.nice = std::clamp(Setting<int32_t>("worker_nice", 0), 0, 19);
		return config;
	}

	// The processing options the settings pick, and what is used instead when this build or machine can't
	// (main thread: the GPU is set up here the first time)
	void LogProcessingOptions(const UgcJobs::Settings& settings) {
		const auto made = UgcJobs::MadeWith(settings);
		LOG("Processing options: %s", UgcProcessOptions::ToString(made).c_str());
		if (UgcRays::Resolve(settings.ao.rays) != settings.ao.rays) {
			LOG("ray_backend=%s can't be used (%s): embree instead", std::string(UgcRays::Name(settings.ao.rays)).c_str(), UgcRays::Problem(settings.ao.rays).c_str());
		}
		if (!UgcRender::Available(settings.icon.denoise)) LOG("denoise=%s can't be used (the server was built without DLU_OIDN): off instead", std::string(UgcRender::Name(settings.icon.denoise)).c_str());
	}

	// Whether LU Toolbox in Blender can be used, logged at start and when it changes (and the processor setting's
	// fallback): every make that asks for it and can't have it logs why too
	void LogToolbox(const UgcJobs::Settings& settings, const std::string& problem) {
		static std::optional<std::string> logged;
		if (logged && *logged == problem + settings.processor) return;
		logged = problem + settings.processor;
		if (problem.empty()) {
			LOG("toolbox-blender can be used%s", settings.processor == UgcProcessOptions::TOOLBOX_BLENDER ? " and makes the models (processor=toolbox-blender)" : "");
		} else if (settings.processor == UgcProcessOptions::TOOLBOX_BLENDER) {
			LOG("processor=toolbox-blender can't be used (%s): models are made natively", problem.c_str());
		} else {
			LOG("toolbox-blender can't be used (%s)", problem.c_str());
		}
	}

	UgcProcessor::Limits ReadLimits() {
		UgcProcessor::Limits limits;
		const auto cores = std::max<unsigned>(std::thread::hardware_concurrency(), 1);
		limits.maxCpus = std::clamp(Setting<double>("max_cpu_percent", 0.0), 0.0, 100.0) / 100.0 * cores;
		limits.maxMemoryBytes = Setting<uint64_t>("max_memory_mb", 0) * 1024 * 1024;
		limits.nice = std::clamp(Setting<int32_t>("worker_nice", 0), 0, 19);
		UgcThrottle::ParseHours(Game::config->GetValue("pause_hours"), limits.pauseFromHour, limits.pauseToHour);
		return limits;
	}

	std::vector<std::string> Segments(const std::string& path) {
		std::vector<std::string> segments;
		size_t start = 0;
		while (start < path.size()) {
			auto end = path.find('/', start);
			if (end == std::string::npos) end = path.size();
			if (end > start) segments.push_back(path.substr(start, end - start));
			start = end + 1;
		}
		return segments;
	}

	std::string Lower(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	void NotFound(HTTPReply& reply) {
		reply.status = eHTTPStatusCode::NOT_FOUND;
		reply.message = "Not Found";
		reply.contentType = eContentType::TEXT_PLAIN;
	}

	void ServeFile(HTTPReply& reply, UgcStorage::Kind kind, LWOOBJID id, const std::string& name, eContentType type, bool crossOrigin) {
		switch (g_Processor->Request(kind, id)) {
		case UgcProcessor::Availability::READY:
			break;
		case UgcProcessor::Availability::QUEUED:
			// The client asks again later on 408
			reply.status = eHTTPStatusCode::REQUEST_TIMEOUT;
			reply.message = "Being made";
			reply.contentType = eContentType::TEXT_PLAIN;
			reply.headers.push_back("Retry-After: 5");
			return;
		case UgcProcessor::Availability::UNKNOWN:
			NotFound(reply);
			return;
		}
		// A car or rocket's files are its combination's (shared by every build of the same modules)
		const auto path = g_Storage->File(kind, g_Processor->StorageId(kind, id), name);
		if (!path) {
			NotFound(reply);
			return;
		}
		reply.status = eHTTPStatusCode::OK;
		reply.file = path->string();
		reply.contentType = type;
		reply.headers.push_back("Cache-Control: public, max-age=3600");
		if (crossOrigin) reply.headers.push_back("Access-Control-Allow-Origin: *");
	}

	/**
	 * A model's LXFML straight from its ugc row (it needs no making, so it's never waited for or evicted): gzip
	 * compressed, or its .checksum. The last few are kept in memory.
	 */
	void ServeLxfml(HTTPReply& reply, LWOOBJID id, bool gz) {
		static std::map<LWOOBJID, std::pair<std::string, std::string>> cache; // id -> (gz, checksum); main thread only
		auto it = cache.find(id);
		if (it == cache.end()) {
			const auto model = Database::Get()->GetUgcModel(id);
			const auto lxfml = model ? UgcJobs::LxfmlFromBlob(model->lxfmlData.str()) : std::string();
			if (lxfml.empty()) return NotFound(reply);
			if (cache.size() >= 64) cache.erase(cache.begin());
			it = cache.emplace(id, std::pair{ ZCompression::Gzip(lxfml), UgcFormats::ChecksumXml(lxfml) }).first;
		}
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = gz ? eContentType::APPLICATION_OCTET_STREAM : eContentType::TEXT_PLAIN;
		reply.message = gz ? it->second.first : it->second.second;
		reply.headers.push_back("Cache-Control: public, max-age=60");
	}

	/**
	 * A game client download (3D services): <client_path>/UGCC<dc>/[3DOPTIMIZED/|IMAGE128DDS/]<dc><id><.ext>(.gz|.checksum).
	 * `segments` are the path's parts after client_path.
	 */
	void ServeClientDownload(HTTPReply& reply, const std::vector<std::string>& segments) {
		NotFound(reply);
		if (segments.size() < 2 || segments.size() > 3) return;
		const auto folder = Lower(segments[0]);
		if (!folder.starts_with("ugcc")) return;
		const auto datacenter = folder.substr(4);
		const std::string typeFolder = segments.size() == 3 ? Lower(segments[1]) : "";
		auto file = Lower(segments.back());

		std::string suffix;
		if (file.ends_with(".gz")) suffix = ".gz";
		else if (file.ends_with(".checksum")) suffix = ".checksum";
		else return;
		file.resize(file.size() - suffix.size());
		const auto dot = file.rfind('.');
		if (dot == std::string::npos) return;
		const auto extension = file.substr(dot);
		auto number = file.substr(0, dot);
		// The file name starts with the datacenter id
		if (!number.starts_with(datacenter)) return;
		number = number.substr(datacenter.size());
		const auto id = GeneralUtils::TryParse<uint64_t>(number);
		if (!id || number.empty()) return;
		const auto blueprint = static_cast<LWOOBJID>(*id);

		const auto type = suffix == ".gz" ? eContentType::APPLICATION_OCTET_STREAM : eContentType::TEXT_PLAIN;
		if (extension == ".lxfml" && typeFolder.empty()) {
			ServeLxfml(reply, blueprint, suffix == ".gz");
		} else if (extension == ".nif" && typeFolder == "3doptimized") {
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "model.nif" + suffix, type, false);
		} else if (extension == ".dds" && typeFolder == "image128dds") {
			// A player model's icon, else a modular build's
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "icon.dds" + suffix, type, false);
			if (reply.status == eHTTPStatusCode::NOT_FOUND) ServeFile(reply, UgcStorage::Kind::MODULAR, blueprint, "icon.dds" + suffix, type, false);
		}
		// .hkx: no physics is made
	}

	/**
	 * A game client download without 3D services (UGCUSE3DSERVICES=7:0, the client's default):
	 * BrickModels/UserMade/<id % 1000, 3 digits>/<id, 20 digits><.lxfml|.nif|.hkx|.dds>.sd0 under its UGCSERVERDIR. The
	 * client asked its world for the file's checksum first (UGC_MANIFEST_RESPONSE) and checks the inflated download
	 * against it.
	 */
	void ServeClientSd0(HTTPReply& reply, const std::string& bucket, const std::string& name) {
		NotFound(reply);
		auto file = Lower(name);
		if (!file.ends_with(".sd0")) return;
		file.resize(file.size() - 4);
		const auto dot = file.find('.');
		if (dot == std::string::npos) return;
		const auto extension = file.substr(dot);
		const auto number = file.substr(0, dot);
		const auto id = GeneralUtils::TryParse<uint64_t>(number);
		if (!id || number.empty() || GeneralUtils::TryParse<uint64_t>(bucket) != *id % 1000) return;
		const auto blueprint = static_cast<LWOOBJID>(*id);
		if (extension == ".lxfml") {
			// Straight from the ugc row, like the 3D services download; the last few are kept
			static std::map<LWOOBJID, std::string> cache; // main thread only
			auto it = cache.find(blueprint);
			if (it == cache.end()) {
				const auto model = Database::Get()->GetUgcModel(blueprint);
				const auto lxfml = model ? UgcJobs::LxfmlFromBlob(model->lxfmlData.str()) : std::string();
				if (lxfml.empty()) return;
				if (cache.size() >= 64) cache.erase(cache.begin());
				it = cache.emplace(blueprint, Sd0::Compress(lxfml)).first;
			}
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			reply.message = it->second;
			reply.headers.push_back("Cache-Control: public, max-age=60");
		} else if (extension == ".nif") {
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "model.nif.sd0", eContentType::APPLICATION_OCTET_STREAM, false);
		} else if (extension == ".dds") {
			// A player model's icon, else a car or rocket's (its combination's)
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "icon.dds.sd0", eContentType::APPLICATION_OCTET_STREAM, false);
			if (reply.status == eHTTPStatusCode::NOT_FOUND) ServeFile(reply, UgcStorage::Kind::MODULAR, blueprint, "icon.dds.sd0", eContentType::APPLICATION_OCTET_STREAM, false);
		}
		// .hkx: no physics is made, the client makes its own
	}

	void RegisterRoutes() {
		// The configured path, and the one the 1.10.64 client uses whatever its boot.cfg says (its built-in patch server
		// folder, lwoclient/UserBrickModels; see docs/UgcServer.md)
		std::set<std::string> bases;
		for (const auto& path : { Game::config->GetValue("client_path").empty() ? std::string("/ugc") : Game::config->GetValue("client_path"), std::string("/lwoclient/UserBrickModels") }) {
			const auto prefix = Segments(Lower(path));
			std::string base;
			for (const auto& segment : prefix) base += "/" + segment;
			if (!bases.insert(base).second) continue;
			const auto clientRoute = [prefixSize = prefix.size()](HTTPReply& reply, const HTTPContext& context) {
				auto segments = Segments(context.originalPath);
				if (segments.size() <= prefixSize) {
					NotFound(reply);
					return;
				}
				segments.erase(segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(prefixSize));
				ServeClientDownload(reply, segments);
				// What the client asked for and what it got, to see how it loads models
				LOG("Client download %s -> %i%s", context.originalPath.c_str(), static_cast<int>(reply.status), reply.file.empty() ? "" : " (file)");
			};
			Game::web.RegisterHTTPRoute({ .path = base + "/:folder/:file", .method = eHTTPMethod::GET, .middleware = {}, .handle = clientRoute });
			Game::web.RegisterHTTPRoute({ .path = base + "/:folder/:type/:file", .method = eHTTPMethod::GET, .middleware = {}, .handle = clientRoute });
			LOG("Serving the client's downloads under %s/UGCC<datacenter>/", base.c_str());
		}

		// Without 3D services: <UGCSERVERDIR>/BrickModels/UserMade/<bucket>/<file>.sd0, where UGCSERVERDIR is client_path
		// when boot.cfg sets it, else the client's default <PATCHSERVERDIR>/UserBrickModels (any patch folder), or empty
		std::set<std::string> sd0Bases = { "/:patchdir/userbrickmodels", "" };
		{
			std::string base;
			for (const auto& segment : Segments(Lower(Game::config->GetValue("client_path").empty() ? std::string("/ugc") : Game::config->GetValue("client_path")))) base += "/" + segment;
			sd0Bases.insert(base);
		}
		for (const auto& base : sd0Bases) {
			Game::web.RegisterHTTPRoute({ .path = base + "/brickmodels/usermade/:bucket/:file", .method = eHTTPMethod::GET, .middleware = {},
				.handle = [](HTTPReply& reply, const HTTPContext& context) {
					const auto segments = Segments(context.originalPath);
					if (segments.size() < 2) return NotFound(reply);
					ServeClientSd0(reply, segments[segments.size() - 2], segments.back());
					LOG("Client download %s -> %i%s", context.originalPath.c_str(), static_cast<int>(reply.status), reply.file.empty() ? "" : " (file)");
				} });
			LOG("Serving the client's sd0 downloads under %s/BrickModels/UserMade/", base.c_str());
		}

		// Previews for the dashboard
		Game::web.RegisterHTTPRoute({ .path = "/files/:kind/:id/:file", .method = eHTTPMethod::GET, .middleware = {},
			.handle = [](HTTPReply& reply, const HTTPContext& context) {
				const auto segments = Segments(context.originalPath);
				NotFound(reply);
				reply.headers.push_back("Access-Control-Allow-Origin: *");
				if (segments.size() != 4) return;
				const auto id = GeneralUtils::TryParse<uint64_t>(segments[2]);
				const auto kind = segments[1] == "model" ? std::optional(UgcStorage::Kind::MODEL) : segments[1] == "modular" ? std::optional(UgcStorage::Kind::MODULAR) : std::nullopt;
				const auto& name = segments[3];
				static const std::set<std::string> PREVIEWS = { "icon.png", "model.nif", "model.noao.nif", "stats.json", "combo.json",
					"previous.icon.png", "previous.model.nif", "previous.model.noao.nif", "previous.stats.json" };
				if (!id || !kind || !PREVIEWS.contains(name)) return;
				reply.headers.clear();
				const auto type = name.ends_with(".png") ? eContentType::IMAGE_PNG : name.ends_with(".json") ? eContentType::APPLICATION_JSON : eContentType::APPLICATION_OCTET_STREAM;
				if (name.ends_with(".nif")) {
					// Stored compressed: inflated for the dashboard
					if (g_Processor->Request(*kind, static_cast<LWOOBJID>(*id)) != UgcProcessor::Availability::READY) return;
					auto nif = g_Storage->ReadNif(*kind, static_cast<LWOOBJID>(*id), name);
					if (!nif) return;
					reply.status = eHTTPStatusCode::OK;
					reply.contentType = type;
					reply.message = std::move(*nif);
					reply.headers.push_back("Access-Control-Allow-Origin: *");
					reply.headers.push_back("Cache-Control: no-cache");
					return;
				}
				ServeFile(reply, *kind, static_cast<LWOOBJID>(*id), name, type, true);
				// Made again since: the newest files differ, so they mustn't be cached as long as the client's
				if (reply.status == eHTTPStatusCode::OK) {
					std::erase_if(reply.headers, [](const std::string& header) { return header.starts_with("Cache-Control"); });
					reply.headers.push_back("Cache-Control: no-cache");
				}
			} });

		// For the dashboard only (it sends the master password): icon previews, drawing icons again, deleting files
		const auto admin = [](HTTPReply& reply, const HTTPContext& context, std::optional<nlohmann::json>& body) {
			const auto& key = context.GetHeader("X-Ugc-Admin-Key");
			bool same = !g_AdminKey.empty() && key.size() == g_AdminKey.size();
			unsigned char diff = 0;
			for (size_t i = 0; same && i < key.size(); i++) diff |= static_cast<unsigned char>(key[i] ^ g_AdminKey[i]);
			reply.contentType = eContentType::APPLICATION_JSON;
			reply.headers.push_back("Cache-Control: no-store");
			if (!same || diff != 0) {
				reply.status = eHTTPStatusCode::FORBIDDEN;
				reply.message = R"({"success":false,"error":"forbidden"})";
				return false;
			}
			body = nlohmann::json::parse(context.body.empty() ? std::string("{}") : context.body, nullptr, false);
			if (!body->is_object()) {
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = R"({"success":false,"error":"the body isn't a JSON object"})";
				return false;
			}
			return true;
		};

		Game::web.RegisterHTTPRoute({ .path = "/admin/preview", .method = eHTTPMethod::POST, .middleware = {},
			.handle = [admin](HTTPReply& reply, const HTTPContext& context) {
				std::optional<nlohmann::json> body;
				if (!admin(reply, context, body)) return;
				const auto values = UgcIconParams::Parse(body->value("values", nlohmann::json::object()).dump());
				const auto kind = body->value("kind", std::string("modular")) == "model" ? UgcStorage::Kind::MODEL : UgcStorage::Kind::MODULAR;
				const auto id = GeneralUtils::TryParse<LWOOBJID>(body->value("id", std::string("0"))).value_or(0);
				auto deferred = Web::Defer(reply, context);
				std::string error;
				if (!g_Processor->QueuePreview(kind, id, body->value("modules", std::string()), values, deferred, error)) {
					HTTPReply out;
					out.status = eHTTPStatusCode::BAD_REQUEST;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = nlohmann::json{ { "success", false }, { "error", error } }.dump();
					deferred.Send(std::move(out));
				}
			} });

		Game::web.RegisterHTTPRoute({ .path = "/admin/assembly", .method = eHTTPMethod::POST, .middleware = {},
			.handle = [admin](HTTPReply& reply, const HTTPContext& context) {
				std::optional<nlohmann::json> body;
				if (!admin(reply, context, body)) return;
				auto deferred = Web::Defer(reply, context);
				std::string error;
				if (!g_Processor->QueueAssembly(body->value("modules", std::string()), deferred, error)) {
					HTTPReply out;
					out.status = eHTTPStatusCode::BAD_REQUEST;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = nlohmann::json{ { "success", false }, { "error", error } }.dump();
					deferred.Send(std::move(out));
				}
			} });

		Game::web.RegisterHTTPRoute({ .path = "/admin/regenerate-icons", .method = eHTTPMethod::POST, .middleware = {},
			.handle = [admin](HTTPReply& reply, const HTTPContext& context) {
				std::optional<nlohmann::json> body;
				if (!admin(reply, context, body)) return;
				const auto queued = g_Processor->RegenerateIcons(body->value("kind", std::string()));
				reply.status = eHTTPStatusCode::OK;
				reply.message = nlohmann::json{ { "success", true }, { "queued", queued } }.dump();
			} });

		Game::web.RegisterHTTPRoute({ .path = "/admin/delete", .method = eHTTPMethod::POST, .middleware = {},
			.handle = [admin](HTTPReply& reply, const HTTPContext& context) {
				std::optional<nlohmann::json> body;
				if (!admin(reply, context, body)) return;
				UgcProcessor::DeleteRequest request;
				request.kind = body->value("kind", std::string("model")) == "modular" ? UgcStorage::Kind::MODULAR : UgcStorage::Kind::MODEL;
				if (const auto ids = body->find("ids"); ids != body->end() && ids->is_array()) {
					for (const auto& id : *ids) {
						const auto parsed = GeneralUtils::TryParse<LWOOBJID>(id.is_string() ? id.get<std::string>() : id.dump());
						if (parsed) request.ids.push_back(*parsed);
					}
				}
				request.all = body->value("all", false);
				request.olderThanDays = body->value("olderThanDays", int64_t{ 0 });
				request.unusedDays = body->value("unusedDays", int64_t{ 0 });
				const auto after = body->value("after", std::string("on_demand"));
				request.after = after == "now" ? UgcProcessor::eAfterDelete::NOW : after == "gone" ? UgcProcessor::eAfterDelete::GONE : UgcProcessor::eAfterDelete::ON_DEMAND;
				const auto result = g_Processor->Delete(request);
				LOG(result.started ? "Purge started for the dashboard (%zu item(s) to look at, 0: all)" : "Purge not started: %zu", result.started ? result.queued : result.notes.size());
				reply.status = eHTTPStatusCode::OK;
				reply.message = nlohmann::json{ { "success", true }, { "deleted", result.deleted }, { "bytes", result.bytes }, { "busy", result.busy }, { "notes", result.notes },
					{ "started", result.started }, { "queued", result.queued } }.dump();
			} });

		Game::web.RegisterHTTPRoute({ .path = "/status", .method = eHTTPMethod::GET, .middleware = {},
			.handle = [](HTTPReply& reply, const HTTPContext&) {
				reply.status = eHTTPStatusCode::OK;
				reply.contentType = eContentType::APPLICATION_JSON;
				reply.message = g_Processor->Status().dump();
				reply.headers.push_back("Access-Control-Allow-Origin: *");
				reply.headers.push_back("Cache-Control: no-store");
			} });
	}

	std::filesystem::path ResPath() {
		const auto client = Game::config->GetValue("client_location");
		return client.empty() ? std::filesystem::path{} : std::filesystem::path(client) / "res";
	}

	// Brick files through the client's assets, packed or unpacked (loose files first, as the game reads them). Made on
	// the main thread; the AssetManager is only read after that, so the workers may call it at once.
	UgcBricks::FileReader ClientReader() {
		const auto client = Game::config->GetValue("client_location");
		if (client.empty()) return {};
		std::shared_ptr<AssetManager> assets;
		try {
			assets = std::make_shared<AssetManager>(client);
		} catch (const std::exception& e) {
			LOG("Couldn't open the client's assets at %s (%s); reading loose files only", client.c_str(), e.what());
			return {};
		}
		return [assets](const std::string& relative) -> std::optional<std::string> {
			char* data = nullptr;
			uint32_t length = 0;
			try {
				if (!assets->GetFile(relative, &data, &length)) return std::nullopt;
			} catch (const std::exception&) {
				return std::nullopt; // a pack the index names but that isn't there
			}
			std::string out(data, length);
			free(data);
			return out;
		};
	}

	// Command line tools: make one model's or modular build's files into a folder, without a database
	int MakeFromCommandLine(const std::string& mode, const std::string& input, const std::filesystem::path& output, const std::string& options) {
		const auto res = ResPath();
		auto settings = ReadSettings();
		UgcProcessOptions::Choice choice;
		if (!UgcProcessOptions::Parse(options, choice)) {
			std::cerr << "Unknown processing options \"" << options << "\" (ray backend embree, hiprt or embree-gpu; denoise off or oidn; processor native or toolbox-blender)\n";
			return EXIT_FAILURE;
		}
		UgcJobs::ApplyOptions(settings, choice);
		if (UgcRays::Resolve(settings.ao.rays) != settings.ao.rays) {
			std::cerr << "ray_backend=" << UgcRays::Name(settings.ao.rays) << " can't be used (" << UgcRays::Problem(settings.ao.rays) << "): embree instead\n";
		}
		UgcBricks::BrickLibrary library(res, 0, ClientReader());
		if (!library.LoadMaterials()) std::cerr << "Couldn't read Materials.xml from " << (res / "brickdb.zip") << "; bricks will be grey\n";
		UgcJobs::Outcome outcome;
		const auto start = std::chrono::steady_clock::now();
		const double cpuStart = UgcThrottle::JobCpuSeconds();
		if (mode == "--make-model") {
			const auto data = UgcBricks::ReadFile(input);
			if (!data) {
				std::cerr << "Can't read " << input << "\n";
				return EXIT_FAILURE;
			}
			// LU Toolbox in Blender when asked for and it can run (Blender is started for this model and stopped after)
			UgcToolbox::Worker toolbox;
			toolbox.Configure(ReadToolboxConfig());
			std::string why;
			if (UgcToolbox::Resolve(settings.processor, UgcToolbox::Problem(toolbox.GetConfig()), why) == UgcProcessOptions::TOOLBOX_BLENDER) {
				// A file name has no object id: its name hashed names Blender's files
				outcome = UgcJobs::ProcessModelToolbox(*data, library, settings, toolbox, UgcModularKey::StorageId(input));
				toolbox.Stop();
				// Blender's CPU time is charged to this thread (JobCpuSeconds below)
			} else {
				if (!why.empty()) std::cerr << "toolbox-blender can't be used (" << why << "): made natively\n";
				outcome = UgcJobs::ProcessModel(*data, library, settings);
			}
		} else {
			CDClientDatabase::Connect(FdbSnapshot::Resolve(BinaryPathFinder::GetBinaryDir() / "resServer").sqlite.string());
			UgcJobs::ModularInput modular;
			std::string error;
			if (!UgcCdClient::GatherModular(input, modular, error)) {
				std::cerr << error << "\n";
				return EXIT_FAILURE;
			}
			outcome = UgcJobs::ProcessModular(modular, res, settings);
		}
		const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		if (!outcome.ok) {
			std::cerr << "Failed: " << outcome.error << "\n";
			return EXIT_FAILURE;
		}
		std::filesystem::create_directories(output);
		for (const auto& [name, data] : outcome.files) {
			std::ofstream(output / name, std::ios::binary).write(data.data(), static_cast<std::streamsize>(data.size()));
		}
		const double cpuMs = (UgcThrottle::JobCpuSeconds() - cpuStart) * 1000.0;
		std::cout << "Made " << outcome.files.size() << " files in " << ms << " ms (" << cpuMs << " ms CPU)" << (outcome.options.empty() ? "" : " with " + outcome.options) <<
			(outcome.note.empty() ? "" : ": " + outcome.note) << "\n";
		return EXIT_SUCCESS;
	}
}

int main(int argc, char** argv) {
	// Crash dumps and the log file share this name (Crash_UgcServer_<start time>_<pid>.log), like the other servers
	const auto serviceName = "UgcServer_" + std::to_string(time(nullptr));
	Diagnostics::SetProcessName(serviceName);
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();
	Diagnostics::SetProduceMemoryDump(true);
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("ugcconfig.ini");

	// UgcServer --make-model <file.lxfml> <folder> [options] or --make-modular "1:4713+1:4714+1:4715" <folder> [options];
	// options: processing options over the settings (UgcProcessOptions), e.g. embree oidn, or toolbox-blender
	if (argc >= 4 && (std::string(argv[1]) == "--make-model" || std::string(argv[1]) == "--make-modular")) {
		std::string options;
		for (int i = 4; i < argc; i++) options += std::string(options.empty() ? "" : " ") + argv[i];
		return MakeFromCommandLine(argv[1], argv[2], argv[3], options);
	}

	// Like the other servers: logs/UgcServer/UgcServer_<start time>.log
	Server::SetupLogger(serviceName, "UgcServer");
	if (!Game::logger) return EXIT_FAILURE;
	// Crash reports go where the dashboard lists them (Server Health, crash dumps)
	if (!Game::config->GetValue("dump_folder").empty()) Diagnostics::SetOutDirectory(Game::config->GetValue("dump_folder"));
	Game::config->LogSettings();
	LOG("Starting UGC Server");
	LOG("Version: %s", std::string(BuildInfo::buildString).c_str());

	const auto res = ResPath();
	if (res.empty() || !std::filesystem::exists(res)) {
		LOG("client_location is not set or has no res folder; the UGC server needs the client's brick data");
		return EXIT_FAILURE;
	}

	try {
		CDClientDatabase::Connect(FdbSnapshot::Resolve(BinaryPathFinder::GetBinaryDir() / "resServer").sqlite.string());
	} catch (std::exception& ex) {
		LOG("Failed to connect to CDClient database: %s", ex.what());
		return EXIT_FAILURE;
	}

	try {
		Database::Connect();
	} catch (std::exception& ex) {
		LOG("Failed to connect to the database: %s", ex.what());
		return EXIT_FAILURE;
	}

	// Settings edited on the dashboard (server_config table) are layered over the files from here on
	Game::config->SetDatabaseSync(ConfigSync::Sync);

	std::string masterIP = "localhost";
	uint32_t masterPort = 1000;
	std::string masterPassword;
	if (const auto masterInfo = Database::Get()->GetMasterInfo()) {
		masterIP = masterInfo->ip;
		masterPort = masterInfo->port;
		masterPassword = masterInfo->password;
	}
	g_AdminKey = masterPassword;

	// The master starts it again when this link drops
	g_Server = new dServer(masterIP, Setting<uint32_t>("net_port", 2012), 0, 16, false, false, Game::logger, masterIP, masterPort,
		ServiceType::UGC, Game::config, &Game::lastSignal, masterPassword);
	Game::server = g_Server;

	UgcBricks::BrickLibrary library(res, 0, ClientReader());
	if (!library.LoadMaterials()) LOG("Couldn't read Materials.xml from %s; bricks will be grey", (res / "brickdb.zip").string().c_str());

	auto outputDir = std::filesystem::path(Game::config->GetValue("ugc_output_dir").empty() ? "ugc" : Game::config->GetValue("ugc_output_dir"));
	if (outputDir.is_relative()) outputDir = BinaryPathFinder::GetBinaryDir() / outputDir;
	UgcStorage storage(outputDir);
	g_Storage = &storage;

	UgcProcessor::Config processorConfig;
	processorConfig.pollIntervalMs = std::max<uint32_t>(Setting<uint32_t>("poll_interval_ms", 2000), 100);
	processorConfig.pollBatch = std::max<uint32_t>(Setting<uint32_t>("poll_batch", 32), 1);
	processorConfig.maxAttempts = std::max<uint32_t>(Setting<uint32_t>("max_attempts", 3), 1);
	processorConfig.maxStorageBytes = Setting<uint64_t>("ugc_max_storage_mb", 2048) * 1024 * 1024;
	const auto threads = Setting<uint32_t>("worker_threads", 0);
	processorConfig.threads = threads > 0 ? threads : std::max<size_t>(std::thread::hardware_concurrency() / 2, 1);
	UgcProcessor processor(processorConfig, storage, library, ReadSettings());
	processor.Configure(ReadSettings(), ReadLimits());
	processor.ConfigureToolbox(ReadToolboxConfig());
	LogProcessingOptions(ReadSettings());
	LogToolbox(ReadSettings(), processor.ToolboxProblem());
	g_Processor = &processor;
	// Sent with the traffic reports to the dashboard (Diagnostics)
	TrafficStats::Local().SetGauge("workers_busy", [&processor] { return static_cast<double>(processor.Busy()); });
	TrafficStats::Local().SetGauge("workers_queued", [&processor] { return static_cast<double>(processor.Queued()); });
	TrafficStats::Local().SetGauge("workers_threads", [&processor] { return static_cast<double>(processor.Threads()); });
	// Read on this thread, when a traffic report is taken (dServer::ReceiveFromMaster)
	const std::vector<std::pair<std::string, std::function<double()>>> ugcGauges{
		{ "ugc_made_total", [&processor] { return static_cast<double>(processor.Made()); } },
		{ "ugc_failed_total", [&processor] { return static_cast<double>(processor.Failed()); } },
		{ "ugc_evicted_total", [&processor] { return static_cast<double>(processor.Evicted()); } },
		{ "ugc_stored_bytes", [&processor] { return static_cast<double>(processor.StoredBytes()); } },
		{ "ugc_max_storage_bytes", [&processor] { return static_cast<double>(processor.MaxStorageBytes()); } },
	};
	for (const auto& [name, gauge] : ugcGauges) TrafficStats::Local().SetGauge(name, gauge);
	TrafficStats::Local().SetGauge("cpu_percent", [&processor] { return processor.CpuPercent(); });
	TrafficStats::Local().SetGauge("memory_mb", [] { return static_cast<double>(UgcProcessor::ResidentBytes()) / (1024.0 * 1024.0); });
	TrafficStats::Local().SetGauge("job_memory_mb", [&processor] { return static_cast<double>(processor.JobMemory()) / (1024.0 * 1024.0); });
	TrafficStats::Local().SetGauge("throttled", [&processor] { return processor.Throttled() ? 1.0 : 0.0; });

	const auto listenIp = Game::config->GetValue("listen_ip").empty() ? std::string("0.0.0.0") : Game::config->GetValue("listen_ip");
	const auto port = Setting<uint32_t>("port", 2008);
	if (!Game::web.Startup(listenIp, port)) {
		LOG("Failed to start the web server on %s:%u", listenIp.c_str(), port);
		return EXIT_FAILURE;
	}
	RegisterRoutes();
	processor.Start();
	LOG("UGC Server started on %s:%u, files in %s", listenIp.c_str(), port, outputDir.string().c_str());

	auto lastTick = std::chrono::steady_clock::now();
	auto lastConfigure = lastTick;
	while (!Game::ShouldShutdown()) {
		// The poll's wait for network traffic is also this loop's sleep
		Game::web.ReceiveRequests(10);
		const auto now = std::chrono::steady_clock::now();
		if (now - lastTick < std::chrono::milliseconds(16)) continue;
		lastTick = now;
		Profiler::FrameScope frame;

		Packet* packet = g_Server->ReceiveFromMaster();
		while (packet) {
			// Live update (docs/LiveUpdate.md): finish the running jobs, then stop; master starts the new build's server
			RakNet::BitStream inStream(packet->data, packet->length, false);
			LUBitStream header;
			if (header.ReadHeader(inStream) && header.connectionType == ServiceType::MASTER &&
				static_cast<MessageType::Master>(header.internalPacketID) == MessageType::Master::LIVE_UPDATE_RETIRE && !processor.IsDraining()) {
				LOG("Live update: finishing the running jobs, then stopping");
				processor.Drain();
			}
			g_Server->DeallocateMasterPacket(packet);
			packet = g_Server->ReceiveFromMaster();
		}
		processor.Update();
		if (processor.Drained()) {
			LOG("Live update: the running jobs are done; stopping");
			Game::lastSignal = -1;
		}
		// Worlds showing a model whose mesh changed tell their clients (docs/UgcServer.md, "Models without 3D services")
		if (auto changed = processor.TakeChangedMeshes(); !changed.empty()) {
			for (size_t start = 0; start < changed.size(); start += UgcModelsMade::MAX_MODELS) {
				UgcModelsMade made;
				made.blueprintIds.assign(changed.begin() + start, changed.begin() + std::min(changed.size(), start + UgcModelsMade::MAX_MODELS));
				MasterPackets::SendToMaster(made, g_Server);
			}
		}
		// Settings the dashboard changed arrive as a config reload; pick them up
		if (now - lastConfigure >= std::chrono::seconds(5)) {
			lastConfigure = now;
			const auto settings = ReadSettings();
			processor.Configure(settings, ReadLimits());
			processor.ConfigureToolbox(ReadToolboxConfig());
			LogToolbox(settings, processor.ToolboxProblem());
		}
	}

	LOG("Stopping the UGC server");
	processor.Stop();
	TrafficStats::Local().SetGauge("workers_busy", nullptr);
	TrafficStats::Local().SetGauge("workers_queued", nullptr);
	TrafficStats::Local().SetGauge("workers_threads", nullptr);
	for (const auto& [name, gauge] : ugcGauges) TrafficStats::Local().SetGauge(name, nullptr);
	for (const auto* gauge : { "cpu_percent", "memory_mb", "job_memory_mb", "throttled" }) TrafficStats::Local().SetGauge(gauge, nullptr);
	Game::web.Shutdown();
	g_Processor = nullptr;
	Database::Destroy("UgcServer");
	delete g_Server;
	g_Server = nullptr;
	Game::server = nullptr;
	delete Game::logger;
	Game::logger = nullptr;
	delete Game::config;
	Game::config = nullptr;
	return EXIT_SUCCESS;
}
