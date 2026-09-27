#include "ServerRoutes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#include "RouteUtils.h"
#include "ServerState.h"
#include "Traffic.h"
#include "dServer.h"
#include "Background.h"
#include "PlayerActions.h"
#include "BinaryPathFinder.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "DashboardRoutes.h"
#include "LogBundle.h"
#include "Workers.h"

#ifdef __linux__
#include <unistd.h>
#endif

using namespace RouteUtils;

namespace {
	namespace fs = std::filesystem;
	constexpr auto SAMPLE_INTERVAL = std::chrono::minutes(1);
	constexpr size_t MAX_MATCHES = 1000;
	std::chrono::steady_clock::time_point g_NextSample{};

	uint32_t OwnPid() {
#ifdef __linux__
		return static_cast<uint32_t>(getpid());
#else
		return 0;
#endif
	}

	// CPU seconds of each process when it was last looked at, for its CPU use since then
	std::map<uint32_t, std::pair<double, std::chrono::steady_clock::time_point>> g_LastCpu;

	// All DarkflameServer processes from this build together, in kB
	uint64_t ServerMemoryKb() {
		uint64_t total = 0;
		for (const auto& process : ServerRoutes::Processes()) total += process.memoryKb;
		return total;
	}

	nlohmann::json ProcessJson(const ServerRoutes::Process& p) {
		return { {"pid", p.pid}, {"memory_kb", p.memoryKb}, {"cpu_percent", std::round(p.cpuPercent * 10) / 10}, {"started_at", p.startedAt} };
	}

	// The UGC server's queue in the database (its work list), from the model and modular build tables
	nlohmann::json UgcCounts() {
		const auto counts = [](const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& rows) {
			nlohmann::json out = nlohmann::json::object();
			for (const auto state : magic_enum::enum_values<IUgc::eProcessState>()) out[IUgc::ProcessStateName(state)] = 0;
			for (const auto& [state, count] : rows) {
				out[IUgc::ProcessStateName(state)] = count;
			}
			return out;
		};
		return { {"model", counts(Database::Get()->GetUgcProcessCounts())}, {"modular", counts(Database::Get()->GetModularBuildProcessCounts())} };
	}

	// What /api/servers and the home page show of the UGC server
	nlohmann::json UgcSummary(bool withCounts) {
		const auto state = ServerState::GetServerStateJson()["ugc"];
		nlohmann::json out = state;
		uint32_t pid = 0;
		{
			std::lock_guard lock(ServerState::g_StatusMutex);
			pid = ServerState::g_UgcPid;
		}
		out["pid"] = pid;
		// Up since its process started when this machine runs it (the dashboard may have started after it)
		if (pid && out.value("online", false)) {
			for (const auto& process : ServerRoutes::Processes()) {
				if (process.pid == pid && process.startedAt) out["since"] = process.startedAt;
			}
		}
		// Its traffic report (every few seconds, via master) carries its workers, totals and storage
		const auto traffic = Traffic::Server("ugc");
		out["gauges"] = traffic.value("gauges", nlohmann::json::object());
		out["last_report"] = traffic.value("last_seen", int64_t{});
		if (withCounts) {
			try {
				out["counts"] = UgcCounts();
			} catch (const std::exception& ex) {
				LOG_DEBUG("Could not count the UGC queue: %s", ex.what());
			}
		}
		return out;
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
		sample.ugcEnabled = state["ugc"].value("enabled", false);
		sample.ugcOnline = state["ugc"].value("online", false);
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

	// Log bundles are built here and deleted once sent (or, where that fails, an hour later)
	fs::path BundleFolder() {
		std::error_code ec;
		auto folder = fs::temp_directory_path(ec);
		if (ec) folder = BinaryPathFinder::GetBinaryDir();
		return folder / "darkflame-log-bundles";
	}

	void ClearOldBundles(const fs::path& folder) {
		std::error_code ec;
		const auto cutoff = fs::file_time_type::clock::now() - std::chrono::hours(1);
		for (const auto& entry : fs::directory_iterator(folder, ec)) {
			if (entry.is_regular_file(ec) && entry.last_write_time(ec) < cutoff) fs::remove(entry.path(), ec);
		}
	}

	std::optional<LogBundle::Filter> BundleFilter(HTTPReply& reply, const HTTPContext& context) {
		std::string error;
		auto filter = LogBundle::Filter::FromQuery([&](const std::string& key) { return QueryValue(context.queryString, key); }, error);
		if (!filter) JsonError(reply, eHTTPStatusCode::BAD_REQUEST, error);
		return filter;
	}

	uint64_t BundleMaxBytes() {
		const auto mb = GeneralUtils::TryParse<uint64_t>(Game::config->GetValue("log_bundle_max_mb")).value_or(512);
		return std::clamp<uint64_t>(mb, 1, 4000) * 1024 * 1024;
	}

	bool PlainFileName(const std::string& name) {
		static const std::regex pattern("^[A-Za-z0-9._-]{1,200}$");
		return std::regex_match(name, pattern) && name.find("..") == std::string::npos;
	}
}

namespace ServerRoutes {
	std::vector<Process> Processes() {
		std::vector<Process> processes;
#ifdef __linux__
		const auto binaryDir = BinaryPathFinder::GetBinaryDir().string();
		const auto ticks = static_cast<double>(sysconf(_SC_CLK_TCK));
		const auto now = std::chrono::steady_clock::now();
		// Process start times are in ticks since boot
		int64_t bootTime = 0;
		{
			std::ifstream stat("/proc/stat");
			std::string line;
			while (std::getline(stat, line)) {
				if (line.starts_with("btime ")) { bootTime = GeneralUtils::TryParse<int64_t>(line.substr(6)).value_or(0); break; }
			}
		}
		std::map<uint32_t, std::pair<double, std::chrono::steady_clock::time_point>> seen;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator("/proc", ec)) {
			const auto pidText = entry.path().filename().string();
			if (pidText.empty() || !std::all_of(pidText.begin(), pidText.end(), ::isdigit)) continue;
			std::error_code linkError;
			const auto exe = fs::read_symlink(entry.path() / "exe", linkError);
			const auto exeText = exe.string();
			if (linkError || !exeText.starts_with(binaryDir) || !exeText.ends_with("Server")) continue;
			Process process;
			process.program = exe.filename().string();
			process.pid = GeneralUtils::TryParse<uint32_t>(pidText).value_or(0);

			std::ifstream status(entry.path() / "status");
			std::string line;
			while (std::getline(status, line)) {
				if (!line.starts_with("VmRSS:")) continue;
				std::istringstream(line.substr(6)) >> process.memoryKb; // "VmRSS:   12345 kB"
				break;
			}

			// stat: "pid (name) state ..." then utime, stime (fields 14, 15) and starttime (22), counted after the name
			std::ifstream statFile(entry.path() / "stat");
			std::string stat((std::istreambuf_iterator<char>(statFile)), std::istreambuf_iterator<char>());
			if (const auto close = stat.rfind(')'); close != std::string::npos) {
				std::istringstream fields(stat.substr(close + 2));
				std::vector<std::string> values{ std::istream_iterator<std::string>(fields), std::istream_iterator<std::string>() };
				if (values.size() > 19 && ticks > 0) {
					const auto cpu = (GeneralUtils::TryParse<double>(values[11]).value_or(0) + GeneralUtils::TryParse<double>(values[12]).value_or(0)) / ticks;
					process.startedAt = bootTime + static_cast<int64_t>(GeneralUtils::TryParse<double>(values[19]).value_or(0) / ticks);
					const auto last = g_LastCpu.find(process.pid);
					if (last != g_LastCpu.end()) {
						const auto wall = std::chrono::duration<double>(now - last->second.second).count();
						if (wall > 0.5) process.cpuPercent = std::max(0.0, (cpu - last->second.first) / wall * 100);
					}
					// Keep the older reading when looked at again too soon for a fair number
					seen[process.pid] = last != g_LastCpu.end() && std::chrono::duration<double>(now - last->second.second).count() <= 0.5 ? last->second : std::pair{ cpu, now };
				}
			}

			// World servers: WorldServer -zone <id> -port <port> -instance <id> ...
			std::ifstream cmdline(entry.path() / "cmdline");
			std::vector<std::string> args;
			for (std::string arg; std::getline(cmdline, arg, '\0');) args.push_back(arg);
			for (size_t i = 0; i + 1 < args.size(); i++) {
				if (args[i] == "-zone") process.zoneId = GeneralUtils::TryParse<uint32_t>(args[i + 1]).value_or(0);
				if (args[i] == "-instance") process.instanceId = GeneralUtils::TryParse<uint32_t>(args[i + 1]).value_or(0);
			}
			processes.push_back(std::move(process));
		}
		g_LastCpu = std::move(seen);
#endif
		return processes;
	}

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
				samples.push_back({ {"time", s.time}, {"players", s.players}, {"worlds", s.worlds}, {"auth", s.authOnline}, {"chat", s.chatOnline}, {"memory_kb", s.memoryKb},
					{"ugc_enabled", s.ugcEnabled}, {"ugc", s.ugcOnline} });
			}
			const auto current = CurrentSample();
			JsonSuccess(reply, { {"from", now - span}, {"to", now}, {"bucket", bucket}, {"samples", samples},
				{"current", { {"players", current.players}, {"worlds", current.worlds}, {"auth", current.authOnline}, {"chat", current.chatOnline}, {"memory_kb", current.memoryKb},
					{"ugc_enabled", current.ugcEnabled}, {"ugc", current.ugcOnline} }} });
		});

	Route(eHTTPMethod::GET, "/api/servers", Perm("health_view"),
		"Every server: {servers: [{key, label, kind, online, since, players, process: {pid, memory_kb, cpu_percent, started_at}, "
		"traffic: {online, last_seen, connections, ping_ms, gauges}}], ugc: {enabled, online, since, pid, gauges, last_report, counts}}. "
		"cpu_percent is of one core since the last time the processes were read",
		[](HTTPReply& reply, const HTTPContext&) {
			const auto state = ServerState::GetServerStateJson();
			auto processes = ServerRoutes::Processes();
			const auto ugc = UgcSummary(true);
			// The process of a server: by pid when known, else the only one of its program (worlds: by zone and instance)
			const auto take = [&](const std::string& program, uint32_t pid, uint32_t zone, uint32_t instance) -> nlohmann::json {
				for (auto it = processes.begin(); it != processes.end(); ++it) {
					const bool match = pid ? it->pid == pid : it->program == program && (program != "WorldServer" || (it->zoneId == zone && it->instanceId == instance));
					if (!match) continue;
					auto json = ProcessJson(*it);
					processes.erase(it);
					return json;
				}
				return nullptr;
			};
			const auto row = [&](const std::string& key, const std::string& kind, bool online, int64_t since, nlohmann::json process) {
				const auto traffic = Traffic::Server(key);
				const auto link = traffic.value("link", nlohmann::json::object());
				return nlohmann::json{ {"key", key}, {"label", traffic.value("label", key)}, {"kind", kind}, {"online", online}, {"since", since}, {"process", process},
					{"traffic", { {"online", traffic.value("online", false)}, {"last_seen", traffic.value("last_seen", int64_t{})},
						{"connections", link.value("connections", 0u)}, {"ping_ms", link.value("ping_ms", 0u)}, {"gauges", traffic.value("gauges", nlohmann::json::object())} }} };
			};
			nlohmann::json servers = nlohmann::json::array();
			const bool masterUp = Game::server && Game::server->GetIsConnectedToMaster();
			servers.push_back(row("master", "MASTER", masterUp, 0, take("MasterServer", 0, 0, 0)));
			servers.push_back(row("auth", "AUTH", state["auth"].value("online", false), state["auth"].value("since", int64_t{}), take("AuthServer", 0, 0, 0)));
			servers.push_back(row("chat", "CHAT", state["chat"].value("online", false), state["chat"].value("since", int64_t{}), take("ChatServer", 0, 0, 0)));
			servers.push_back(row("dashboard", "DASHBOARD", true, 0, take("DashboardServer", OwnPid(), 0, 0)));
			if (ugc.value("enabled", false) || ugc.value("online", false)) {
				auto ugcRow = row("ugc", "UGC", ugc.value("online", false), ugc.value("since", int64_t{}), take("UgcServer", ugc.value("pid", 0u), 0, 0));
				if (ugcRow["process"].is_null()) ugcRow["process"] = take("UgcServer", 0, 0, 0);
				servers.push_back(ugcRow);
			}
			for (const auto& world : state.value("worlds", nlohmann::json::array())) {
				const auto zone = world.value("mapID", 0u), instance = world.value("instanceID", 0u);
				auto worldRow = row("world:" + std::to_string(zone) + ":" + std::to_string(instance), "WORLD", true, 0, take("WorldServer", 0, zone, instance));
				worldRow["players"] = world.value("players", 0u);
				servers.push_back(worldRow);
			}
			// Anything left over, e.g. a world master no longer lists
			for (const auto& process : processes) {
				servers.push_back({ {"key", process.program + ":" + std::to_string(process.pid)}, {"label", process.program}, {"kind", "OTHER"}, {"online", false},
					{"since", 0}, {"process", ProcessJson(process)}, {"traffic", nullptr} });
			}
			JsonSuccess(reply, { {"servers", servers}, {"ugc", ugc} });
		});

	Route(eHTTPMethod::GET, "/api/servers/ugc", Perm("health_view"),
		"The UGC server for the home page: {enabled, online, since, pid, gauges: {workers_busy, workers_queued, workers_threads, ugc_made_total, "
		"ugc_failed_total, ugc_evicted_total, ugc_stored_bytes, ugc_max_storage_bytes}, last_report, counts: {model, modular: {pending, done, failed}}}",
		[](HTTPReply& reply, const HTTPContext&) { JsonSuccess(reply, UgcSummary(true)); });

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

	static const std::string bundleQuery = "Query: from, to (Unix seconds; a file matches when the time from its start to its last write overlaps them), "
		"servers (comma separated: master, auth, chat, dashboard, ugc, world; default all), zones (world zone IDs, comma separated), clone, instance, "
		"crash=1 (crash dumps too), trim=1 (only lines written between from and to), text= (only lines with it, any case), redact=1 (IP addresses become [ip])";

	Route(eHTTPMethod::GET, "/api/logs/bundle/preview", Perm("logs_system"),
		"The log files a bundle would hold. " + bundleQuery + ". Returns {files: [{name, server, zone, zone_name, clone, instance, size, started, written, "
		"crash_dump}] (the first 2000), count, total_size, max_bytes, over_limit (whole files over log_bundle_max_mb), filters_lines}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto filter = BundleFilter(reply, context);
			if (!filter) return;
			const auto files = LogBundle::Select(LogFolder(), DumpFolder(), *filter);
			const auto& zoneNames = ZoneNames();
			nlohmann::json list = nlohmann::json::array();
			uint64_t total = 0;
			for (const auto& file : files) {
				total += file.size;
				if (list.size() >= 2000) continue;
				nlohmann::json item{ {"name", file.archiveName}, {"server", file.name.server}, {"size", file.size}, {"started", file.started},
					{"written", file.written}, {"crash_dump", file.crashDump}, {"zone", nullptr}, {"zone_name", nullptr}, {"clone", nullptr}, {"instance", nullptr} };
				if (file.name.zone) {
					item["zone"] = *file.name.zone;
					item["zone_name"] = zoneNames.value(std::to_string(*file.name.zone), "");
					item["clone"] = *file.name.clone;
					item["instance"] = *file.name.instance;
				}
				list.push_back(std::move(item));
			}
			const auto maxBytes = BundleMaxBytes();
			JsonSuccess(reply, { {"files", list}, {"count", files.size()}, {"total_size", total}, {"max_bytes", maxBytes},
				{"over_limit", !filter->FiltersLines() && total > maxBytes}, {"filters_lines", filter->FiltersLines()} });
		});

	Route(eHTTPMethod::GET, "/api/logs/bundle", Perm("logs_system"),
		"Download log files as one zip file, with a manifest.txt of the filters and files. " + bundleQuery + ". Built in the background and "
		"streamed; 413 when it would hold more than log_bundle_max_mb (before compression)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto filter = BundleFilter(reply, context);
			if (!filter) return;
			// Everything the worker needs is read here: it must not touch the settings or the database
			auto files = LogBundle::Select(LogFolder(), DumpFolder(), *filter);
			const auto maxBytes = BundleMaxBytes();
			uint64_t total = 0;
			for (const auto& file : files) total += file.size;
			if (files.empty()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No log files match");
			if (!filter->FiltersLines() && total > maxBytes) {
				return JsonError(reply, eHTTPStatusCode::PAYLOAD_TOO_LARGE, "These files hold " + LogBundle::SizeText(total) + ", more than the " +
					LogBundle::SizeText(maxBytes) + " a bundle may (log_bundle_max_mb); pick a shorter date range or fewer servers");
			}
			const auto folder = BundleFolder();
			std::error_code ec;
			fs::create_directories(folder, ec);
			ClearOldBundles(folder);
			static uint64_t counter = 0;
			const auto now = std::time(nullptr);
			char stamp[32], made[32];
			const auto local = *std::localtime(&now);
			std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
			std::strftime(made, sizeof(made), "%Y-%m-%d %H:%M:%S", &local);
			const std::string downloadName = std::string("logs_") + stamp + ".zip";
			const auto out = folder / ("bundle_" + std::to_string(context.accountId) + "_" + std::to_string(now) + "_" + std::to_string(++counter) + ".zip");

			std::string header = "DarkflameServer log bundle\nServer version: " PROJECT_VERSION "\nMade: " + std::string(made) + " (server time) by " +
				context.authenticatedUser + "\nFilters: " + filter->ToJson().dump() + "\nSize limit: " + LogBundle::SizeText(maxBytes) + " before compression\n";
			Audit(context, "download_logs", "Downloaded a log bundle: " + std::to_string(files.size()) + " file(s), " + LogBundle::SizeText(total) +
				" of logs; filters " + filter->ToJson().dump());

			Workers::Reply(reply, context, false, [out, downloadName, files = std::move(files), filter = *filter, header, maxBytes](HTTPReply& reply) {
				const auto result = LogBundle::WriteZip(out, files, filter, header, maxBytes);
				if (!result.ok) {
					LOG("Log bundle failed: %s", result.error.c_str());
					return JsonError(reply, result.overLimit ? eHTTPStatusCode::PAYLOAD_TOO_LARGE : eHTTPStatusCode::INTERNAL_SERVER_ERROR, result.error);
				}
				if (result.files == 0) {
					std::error_code ec;
					fs::remove(out, ec);
					return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No lines match these filters");
				}
				LOG("Log bundle %s: %zu file(s), %s of logs, %s zipped", downloadName.c_str(), result.files, LogBundle::SizeText(result.bytesIn).c_str(),
					LogBundle::SizeText(result.archiveSize).c_str());
				reply.file = out.string();
				reply.removeFile = true;
				reply.message.clear();
				reply.status = eHTTPStatusCode::OK;
				reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
				reply.headers.push_back("Content-Disposition: attachment; filename=\"" + downloadName + "\"");
			}, WorkerPool::ePriority::LARGE);
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
