// The UGC server: makes the meshes and icons of what players build and serves them, and the models' LXFML, to the
// game client over HTTP. See docs/UgcServer.md.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>

#include "BinaryPathFinder.h"
#include "CDClientDatabase.h"
#include "ConfigSync.h"
#include "Database.h"
#include "Diagnostics.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
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
#include "UgcJobs.h"
#include "UgcModel.h"
#include "UgcProcessor.h"
#include "UgcStorage.h"

namespace Game {
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dConfig* config = nullptr;
	Game::signal_t lastSignal = 0;
	std::mt19937 randomEngine;
}

namespace {
	dServer* g_Server = nullptr;
	UgcProcessor* g_Processor = nullptr;
	UgcStorage* g_Storage = nullptr;

	template<typename T>
	T Setting(const std::string& key, T fallback) {
		return GeneralUtils::TryParse<T>(Game::config->GetValue(key)).value_or(fallback);
	}

	UgcJobs::Settings ReadSettings() {
		UgcJobs::Settings settings;
		settings.optimize.removeHidden = Setting<int32_t>("remove_hidden_faces", 1) != 0;
		settings.optimize.bakeAo = Setting<int32_t>("bake_ao", 1) != 0;
		settings.optimize.aoStrength = Setting<float>("ao_strength", 0.6f);
		settings.optimize.resolution = Setting<int32_t>("optimize_resolution", 1024);
		settings.icon.size = Setting<int32_t>("icon_size", 128);
		settings.icon.yawDegrees = Setting<float>("icon_yaw", 35.0f);
		settings.icon.pitchDegrees = Setting<float>("icon_pitch", 25.0f);
		settings.icon.margin = Setting<float>("icon_margin", 1.03f);
		settings.modularIcon = settings.icon;
		settings.modularIcon.yawDegrees = Setting<float>("modular_icon_yaw", 35.0f);
		settings.modularIcon.pitchDegrees = Setting<float>("modular_icon_pitch", 20.0f);
		return settings;
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
		const auto path = g_Storage->File(kind, id, name);
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
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "model.lxfml" + suffix, type, false);
		} else if (extension == ".nif" && typeFolder == "3doptimized") {
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "model.nif" + suffix, type, false);
		} else if (extension == ".dds" && typeFolder == "image128dds") {
			// A player model's icon, else a modular build's
			ServeFile(reply, UgcStorage::Kind::MODEL, blueprint, "icon.dds" + suffix, type, false);
			if (reply.status == eHTTPStatusCode::NOT_FOUND) ServeFile(reply, UgcStorage::Kind::MODULAR, blueprint, "icon.dds" + suffix, type, false);
		}
		// .hkx: no physics is made
	}

	void RegisterRoutes() {
		const auto prefix = Segments(Lower(Game::config->GetValue("client_path").empty() ? "/ugc" : Game::config->GetValue("client_path")));
		std::string base;
		for (const auto& segment : prefix) base += "/" + segment;

		const auto clientRoute = [prefixSize = prefix.size()](HTTPReply& reply, const HTTPContext& context) {
			auto segments = Segments(context.originalPath);
			if (segments.size() <= prefixSize) {
				NotFound(reply);
				return;
			}
			segments.erase(segments.begin(), segments.begin() + static_cast<std::ptrdiff_t>(prefixSize));
			ServeClientDownload(reply, segments);
		};
		Game::web.RegisterHTTPRoute({ .path = base + "/:folder/:file", .method = eHTTPMethod::GET, .middleware = {}, .handle = clientRoute });
		Game::web.RegisterHTTPRoute({ .path = base + "/:folder/:type/:file", .method = eHTTPMethod::GET, .middleware = {}, .handle = clientRoute });

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
				if (!id || !kind || (name != "icon.png" && name != "model.nif")) return;
				reply.headers.clear();
				ServeFile(reply, *kind, static_cast<LWOOBJID>(*id), name, name == "icon.png" ? eContentType::IMAGE_PNG : eContentType::APPLICATION_OCTET_STREAM, true);
			} });

		Game::web.RegisterHTTPRoute({ .path = "/status", .method = eHTTPMethod::GET, .middleware = {},
			.handle = [](HTTPReply& reply, const HTTPContext&) {
				reply.status = eHTTPStatusCode::OK;
				reply.contentType = eContentType::APPLICATION_JSON;
				reply.message = g_Processor->Status().dump();
				reply.headers.push_back("Access-Control-Allow-Origin: *");
				reply.headers.push_back("Cache-Control: no-store");
			} });
		LOG("Serving the client's downloads under %s/UGCC<datacenter>/", base.c_str());
	}

	std::filesystem::path ResPath() {
		const auto client = Game::config->GetValue("client_location");
		return client.empty() ? std::filesystem::path{} : std::filesystem::path(client) / "res";
	}

	// Command line tools: make one model's or modular build's files into a folder, without a database
	int MakeFromCommandLine(const std::string& mode, const std::string& input, const std::filesystem::path& output) {
		const auto res = ResPath();
		const auto settings = ReadSettings();
		UgcBricks::BrickLibrary library(res, Setting<uint32_t>("brick_lod", 0));
		if (!library.LoadMaterials()) std::cerr << "Couldn't read Materials.xml from " << (res / "brickdb.zip") << "; bricks will be grey\n";
		UgcJobs::Outcome outcome;
		const auto start = std::chrono::steady_clock::now();
		if (mode == "--make-model") {
			const auto data = UgcBricks::ReadFile(input);
			if (!data) {
				std::cerr << "Can't read " << input << "\n";
				return EXIT_FAILURE;
			}
			outcome = UgcJobs::ProcessModel(*data, library, settings);
		} else {
			CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer/CDServer.sqlite").string());
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
		std::cout << "Made " << outcome.files.size() << " files in " << ms << " ms" << (outcome.note.empty() ? "" : ": " + outcome.note) << "\n";
		return EXIT_SUCCESS;
	}
}

int main(int argc, char** argv) {
	Diagnostics::SetProduceMemoryDump(true);
	std::signal(SIGINT, Game::OnSignal);
	std::signal(SIGTERM, Game::OnSignal);

	Game::config = new dConfig("ugcconfig.ini");

	// UgcServer --make-model <file.lxfml> <folder> or --make-modular "1:4713+1:4714+1:4715" <folder>
	if (argc == 4 && (std::string(argv[1]) == "--make-model" || std::string(argv[1]) == "--make-modular")) {
		return MakeFromCommandLine(argv[1], argv[2], argv[3]);
	}

	// Like the other servers: logs/UgcServer/UgcServer_<start time>.log
	Server::SetupLogger("UgcServer_" + std::to_string(time(nullptr)), "UgcServer");
	if (!Game::logger) return EXIT_FAILURE;
	Game::config->LogSettings();
	LOG("Starting UGC Server");

	const auto res = ResPath();
	if (res.empty() || !std::filesystem::exists(res)) {
		LOG("client_location is not set or has no res folder; the UGC server needs the client's brick data");
		return EXIT_FAILURE;
	}

	try {
		CDClientDatabase::Connect((BinaryPathFinder::GetBinaryDir() / "resServer/CDServer.sqlite").string());
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

	// The master starts it again when this link drops
	g_Server = new dServer(masterIP, Setting<uint32_t>("net_port", 2012), 0, 16, false, false, Game::logger, masterIP, masterPort,
		ServiceType::UGC, Game::config, &Game::lastSignal, masterPassword);
	Game::server = g_Server;

	UgcBricks::BrickLibrary library(res, Setting<uint32_t>("brick_lod", 0));
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
	g_Processor = &processor;
	// Sent with the traffic reports to the dashboard (Diagnostics)
	TrafficStats::Local().SetGauge("workers_busy", [&processor] { return static_cast<double>(processor.Busy()); });
	TrafficStats::Local().SetGauge("workers_queued", [&processor] { return static_cast<double>(processor.Queued()); });
	TrafficStats::Local().SetGauge("workers_threads", [&processor] { return static_cast<double>(processor.Threads()); });

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
	while (!Game::ShouldShutdown()) {
		// The poll's wait for network traffic is also this loop's sleep
		Game::web.ReceiveRequests(10);
		const auto now = std::chrono::steady_clock::now();
		if (now - lastTick < std::chrono::milliseconds(16)) continue;
		lastTick = now;

		Packet* packet = g_Server->ReceiveFromMaster();
		while (packet) {
			g_Server->DeallocateMasterPacket(packet);
			packet = g_Server->ReceiveFromMaster();
		}
		processor.Update();
	}

	LOG("Stopping the UGC server");
	processor.Stop();
	TrafficStats::Local().SetGauge("workers_busy", nullptr);
	TrafficStats::Local().SetGauge("workers_queued", nullptr);
	TrafficStats::Local().SetGauge("workers_threads", nullptr);
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
