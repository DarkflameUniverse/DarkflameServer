#include "ServerRoutes.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#include "RouteUtils.h"
#include "ServerState.h"
#include "Background.h"
#include "PlayerActions.h"
#include "BinaryPathFinder.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	namespace fs = std::filesystem;
	constexpr auto SAMPLE_INTERVAL = std::chrono::minutes(1);
	constexpr size_t MAX_MATCHES = 1000;
	std::chrono::steady_clock::time_point g_NextSample{};

	// All DarkflameServer processes from this build together, in kB (Linux: /proc; elsewhere 0)
	uint64_t ServerMemoryKb() {
#ifdef __linux__
		const auto binaryDir = BinaryPathFinder::GetBinaryDir().string();
		uint64_t total = 0;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator("/proc", ec)) {
			const auto pid = entry.path().filename().string();
			if (pid.empty() || !std::all_of(pid.begin(), pid.end(), ::isdigit)) continue;
			std::error_code linkError;
			const auto exe = fs::read_symlink(entry.path() / "exe", linkError).string();
			if (linkError || !exe.starts_with(binaryDir) || !exe.ends_with("Server")) continue;
			std::ifstream status(entry.path() / "status");
			std::string line;
			while (std::getline(status, line)) {
				if (!line.starts_with("VmRSS:")) continue;
				uint64_t kb = 0;
				std::istringstream(line.substr(6)) >> kb; // "VmRSS:   12345 kB"
				total += kb;
				break;
			}
		}
		return total;
#else
		return 0;
#endif
	}

	IServerHealth::HealthSample CurrentSample() {
		const auto state = ServerState::GetServerStateJson();
		IServerHealth::HealthSample sample;
		sample.time = static_cast<int64_t>(std::time(nullptr));
		sample.players = state["stats"].value("onlinePlayers", 0u);
		sample.worlds = static_cast<uint32_t>(state.value("worlds", nlohmann::json::array()).size());
		sample.authOnline = state["auth"].value("online", false);
		sample.chatOnline = state["chat"].value("online", false);
		sample.memoryKb = ServerMemoryKb();
		return sample;
	}

	fs::path LogFolder() {
		return BinaryPathFinder::GetBinaryDir() / "logs";
	}

	fs::path DumpFolder() {
		fs::path folder = Game::config->GetValue("dump_folder");
		if (folder.empty()) return {};
		return folder.is_absolute() ? folder : BinaryPathFinder::GetBinaryDir() / folder;
	}

	bool PlainFileName(const std::string& name) {
		static const std::regex pattern("^[A-Za-z0-9._-]{1,200}$");
		return std::regex_match(name, pattern) && name.find("..") == std::string::npos;
	}
}

namespace ServerRoutes {
	void Update() {
		const auto now = std::chrono::steady_clock::now();
		// The first sample waits a minute, so auth and chat have time to connect after a start
		if (g_NextSample == std::chrono::steady_clock::time_point{}) g_NextSample = now + SAMPLE_INTERVAL;
		if (now < g_NextSample) return;
		g_NextSample = now + SAMPLE_INTERVAL;
		try {
			Database::Get()->InsertHealthSample(CurrentSample());
		} catch (const std::exception& ex) {
			LOG_DEBUG("Could not record a health sample: %s", ex.what());
		}
	}
}

void RegisterServerRoutes() {
	Route(eHTTPMethod::GET, "/api/health", Perm("health_view"), "Server health over time. Query: ?range=24h|7d|30d. Players and worlds are the highest in each point",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto range = QueryValue(context.queryString, "range");
			const int64_t span = range == "30d" ? 30 * 86400 : range == "7d" ? 7 * 86400 : 86400;
			const int64_t bucket = range == "30d" ? 4 * 3600 : range == "7d" ? 3600 : 300;
			const auto now = static_cast<int64_t>(std::time(nullptr));
			nlohmann::json samples = nlohmann::json::array();
			for (const auto& s : Database::Get()->GetHealthSamples(now - span, now, bucket)) {
				samples.push_back({ {"time", s.time}, {"players", s.players}, {"worlds", s.worlds}, {"auth", s.authOnline}, {"chat", s.chatOnline}, {"memory_kb", s.memoryKb} });
			}
			const auto current = CurrentSample();
			JsonSuccess(reply, { {"from", now - span}, {"to", now}, {"bucket", bucket}, {"samples", samples},
				{"current", { {"players", current.players}, {"worlds", current.worlds}, {"auth", current.authOnline}, {"chat", current.chatOnline}, {"memory_kb", current.memoryKb} }} });
		});

	Route(eHTTPMethod::GET, "/api/crash_dumps", Perm("logs_system"), "Crash dumps in dump_folder, newest first",
		[](HTTPReply& reply, const HTTPContext&) {
			const auto folder = DumpFolder();
			nlohmann::json files = nlohmann::json::array();
			std::error_code ec;
			if (!folder.empty()) {
				for (const auto& entry : fs::directory_iterator(folder, ec)) {
					if (!entry.is_regular_file(ec) || !PlainFileName(entry.path().filename().string())) continue;
					const auto written = std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::clock_cast<std::chrono::system_clock>(fs::last_write_time(entry.path(), ec)).time_since_epoch()).count();
					files.push_back({ {"name", entry.path().filename().string()}, {"size", entry.file_size(ec)}, {"time", written} });
				}
			}
			std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a["time"].template get<int64_t>() > b["time"].template get<int64_t>(); });
			JsonSuccess(reply, { {"folder", folder.string()}, {"files", files} });
		});

	Route(eHTTPMethod::GET, "/api/crash_dumps/:name", Perm("logs_system"), "Download a crash dump",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 2)); // Crash_WorldServer_..._123.log: case matters
			const auto folder = DumpFolder();
			if (folder.empty() || !PlainFileName(name)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not found");
			std::error_code ec;
			if (!fs::is_regular_file(folder / name, ec)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Not found");
			reply.file = (folder / name).string(); // streamed from disk
			reply.message.clear();
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			reply.headers.push_back("Content-Disposition: attachment; filename=\"" + name + "\"");
		});

	Route(eHTTPMethod::GET, "/api/logs/search", Perm("logs_system"),
		"Search the servers' log files. Query: ?q= (text, case-insensitive), &server= (e.g. WorldServer), &files= (newest per server, default 3, max 20). "
		"Runs in the background: returns {requestId}; the result's data has {matches: [{file, line, text}], truncated}",
		[](HTTPReply& reply, const HTTPContext& context) {
			auto query = QueryValue(context.queryString, "q");
			const auto server = QueryValue(context.queryString, "server");
			const auto perServer = std::clamp<int64_t>(GeneralUtils::TryParse<int64_t>(QueryValue(context.queryString, "files")).value_or(3), 1, 20);
			if (query.size() < 2 || query.size() > 200) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Search for 2 to 200 characters");
			std::transform(query.begin(), query.end(), query.begin(), ::tolower);
			const auto folder = LogFolder();
			const auto requestId = PlayerActions::Begin(context.accountId, std::chrono::minutes(5));
			Background::Run("log_search:" + std::to_string(requestId), [folder, query, server, perServer](GameDatabase&) -> nlohmann::json {
				// Newest files of each server (file names: <Server>_..._<time>.log, in the server's own folder under logs/)
				std::map<std::string, std::vector<fs::path>> byServer;
				std::error_code ec;
				for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
					const auto& entry = *it;
					if (!entry.is_regular_file(ec)) continue;
					const auto name = entry.path().filename().string();
					if (!name.ends_with(".log")) continue;
					const auto serverName = name.substr(0, name.find('_'));
					if (!server.empty() && serverName != server) continue;
					byServer[serverName].push_back(entry.path());
				}
				nlohmann::json matches = nlohmann::json::array();
				bool truncated = false;
				for (auto& [serverName, files] : byServer) {
					std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.filename() > b.filename(); });
					for (size_t f = 0; f < std::min<size_t>(files.size(), perServer) && !truncated; f++) {
						std::ifstream in(files[f]);
						std::string line;
						size_t number = 0;
						while (std::getline(in, line)) {
							number++;
							std::string lower = line;
							std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
							if (lower.find(query) == std::string::npos) continue;
							if (matches.size() >= MAX_MATCHES) { truncated = true; break; }
							matches.push_back({ {"file", files[f].filename().string()}, {"line", number}, {"text", line.substr(0, 1000)} });
						}
					}
				}
				return { {"matches", matches}, {"truncated", truncated} };
			}, [requestId](nlohmann::json result, const std::string& error) {
				if (!error.empty()) return PlayerActions::Finish(requestId, { false, "The search failed: " + error });
				const auto count = result["matches"].size();
				PlayerActions::Finish(requestId, { true, std::to_string(count) + " line(s)" + (result["truncated"].get<bool>() ? " (stopped at 1000)" : ""), result });
			});
			JsonSuccess(reply, { {"requestId", requestId} });
		});
}
