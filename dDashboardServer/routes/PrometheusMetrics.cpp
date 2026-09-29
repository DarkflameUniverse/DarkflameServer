#include "PrometheusMetrics.h"
#include "MetricsFormat.h"
#include "Traffic.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "RouteUtils.h"
#include "ReportRoutes.h"
#include "ServerState.h"
#include "Permissions.h"
#include "Strikes.h"
#include "BinaryPathFinder.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "Web.h"
#include "dConfig.h"
#include "dServer.h"
#include "eHTTPMethod.h"
#include "ePlayerReportStatus.h"
#include "magic_enum.hpp"

using namespace RouteUtils;
using namespace MetricsFormat;

namespace {
	using Clock = std::chrono::steady_clock;
	const auto g_Started = Clock::now();
	// A shared metrics_token shorter than this is ignored: it would be too easy to guess
	constexpr size_t MIN_TOKEN_LENGTH = 16;
	constexpr uint32_t CHAT_PAGE = 500;
	constexpr uint32_t CHAT_PAGES = 20; // at most this many pages of new chat are counted per refresh

	std::string g_Text;
	Clock::time_point g_BuiltAt{};

	// Addresses that keep sending wrong tokens wait a minute
	RateLimiter g_Refusals(20, std::chrono::seconds(60));
	std::map<std::string, Clock::time_point> g_BlockedUntil;

	// Chat messages counted since the dashboard started, by channel
	bool g_ChatStarted{};
	uint64_t g_ChatSeen{};
	std::map<std::string, uint64_t> g_ChatMessages{ {"zone", 0}, {"whisper", 0}, {"team", 0}, {"guild", 0}, {"web", 0} };
	std::map<std::string, uint64_t> g_ChatBlocked{ {"zone", 0}, {"whisper", 0}, {"team", 0}, {"guild", 0}, {"web", 0} };

	std::string LowerName(std::string_view name) {
		std::string out(name);
		std::transform(out.begin(), out.end(), out.begin(), ::tolower);
		return out;
	}

	// Resident memory and process count of each DarkflameServer program from this build (Linux: /proc; elsewhere none)
	std::map<std::string, std::pair<uint32_t, uint64_t>> ServerMemory() {
		std::map<std::string, std::pair<uint32_t, uint64_t>> servers;
#ifdef __linux__
		namespace fs = std::filesystem;
		const auto binaryDir = BinaryPathFinder::GetBinaryDir().string();
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator("/proc", ec)) {
			const auto pid = entry.path().filename().string();
			if (pid.empty() || !std::all_of(pid.begin(), pid.end(), ::isdigit)) continue;
			std::error_code linkError;
			const auto exe = fs::read_symlink(entry.path() / "exe", linkError);
			const auto exeText = exe.string();
			if (linkError || !exeText.starts_with(binaryDir) || !exeText.ends_with("Server")) continue;
			std::ifstream status(entry.path() / "status");
			std::string line;
			while (std::getline(status, line)) {
				if (!line.starts_with("VmRSS:")) continue;
				uint64_t kb = 0;
				std::istringstream(line.substr(6)) >> kb; // "VmRSS:   12345 kB"
				auto& server = servers[exe.filename().string()];
				server.first++;
				server.second += kb * 1024;
				break;
			}
		}
#endif
		return servers;
	}

	uint32_t WebSocketClients() {
		uint32_t count = 0;
		for (const auto* connection = Game::web.GetManager().conns; connection; connection = connection->next) {
			if (connection->is_websocket && !connection->is_closing) count++;
		}
		return count;
	}

	// What the dashboard knows without asking the database: master's server list, its own connections, /proc
	void AddLive(MetricsFormat::Writer& w) {
		std::vector<WorldInstanceInfo> worlds;
		bool authUp = false, chatUp = false, ugcEnabled = false, ugcUp = false;
		{
			std::lock_guard lock(ServerState::g_StatusMutex);
			worlds = ServerState::g_WorldInstances;
			authUp = ServerState::g_AuthStatus.online;
			chatUp = ServerState::g_ChatStatus.online;
			ugcEnabled = ServerState::g_UgcEnabled;
			ugcUp = ServerState::g_UgcStatus.online;
		}
		w.Add("darkflame_master_connected", "Whether the dashboard is connected to the master server", "gauge", Game::server && Game::server->GetIsConnectedToMaster() ? 1 : 0);
		w.Add("darkflame_auth_up", "Whether the auth server is up, as master last reported", "gauge", authUp ? 1 : 0);
		w.Add("darkflame_chat_up", "Whether the chat server is up, as master last reported", "gauge", chatUp ? 1 : 0);
		w.Add("darkflame_ugc_enabled", "Whether master starts the UGC server (enable_ugc_server)", "gauge", ugcEnabled ? 1 : 0);
		w.Add("darkflame_ugc_up", "Whether the UGC server is up, as master last reported", "gauge", ugcUp ? 1 : 0);

		uint32_t players = 0;
		std::map<uint32_t, std::pair<std::string, std::pair<uint32_t, uint32_t>>> zones; // id -> name, {instances, players}
		w.Declare("darkflame_instance_players", "Players in one world instance", "gauge");
		for (const auto& world : worlds) {
			players += world.players;
			auto& zone = zones[world.mapID];
			zone.first = world.zoneName;
			zone.second.first++;
			zone.second.second += world.players;
			// Property instances are only told apart by kind: their clone ids lead to the owner
			const std::string kind = world.cloneID != 0 ? "property" : world.isPrivate ? "private" : "public";
			w.Add("darkflame_instance_players", "", "gauge",
				{ {"zone_id", std::to_string(world.mapID)}, {"instance_id", std::to_string(world.instanceID)}, {"kind", kind} }, world.players);
		}
		w.Add("darkflame_players_online", "Players in game (character select included)", "gauge", players);
		w.Add("darkflame_world_instances", "World instances running", "gauge", static_cast<double>(worlds.size()));
		w.Declare("darkflame_zone_players", "Players in a zone, all its instances together", "gauge");
		w.Declare("darkflame_zone_instances", "World instances running for a zone", "gauge");
		for (const auto& [id, zone] : zones) {
			const Labels labels{ {"zone_id", std::to_string(id)}, {"zone_name", zone.first} };
			w.Add("darkflame_zone_players", "", "gauge", labels, zone.second.second);
			w.Add("darkflame_zone_instances", "", "gauge", labels, zone.second.first);
		}

		w.Add("darkflame_dashboard_uptime_seconds", "Seconds since the dashboard started", "gauge",
			static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - g_Started).count()));
		w.Add("darkflame_websocket_clients", "Open dashboard WebSocket connections (browsers and bots)", "gauge", WebSocketClients());

		w.Declare("darkflame_server_memory_bytes", "Resident memory of a kind of server process, all of them together (Linux only)", "gauge");
		w.Declare("darkflame_server_processes", "Running processes of a kind of server (Linux only)", "gauge");
		for (const auto& [server, usage] : ServerMemory()) {
			w.Add("darkflame_server_memory_bytes", "", "gauge", { {"server", server} }, static_cast<double>(usage.second));
			w.Add("darkflame_server_processes", "", "gauge", { {"server", server} }, usage.first);
		}
	}

	// New chat since the last refresh, counted by channel. The first refresh only notes where the log is.
	void CountChat(uint64_t newest) {
		if (!g_ChatStarted) {
			g_ChatStarted = true;
			g_ChatSeen = newest;
			return;
		}
		IChatLog::ChatQuery q;
		q.includePrivate = true;
		q.includeWhispers = true;
		q.limit = CHAT_PAGE;
		for (uint32_t page = 0; page < CHAT_PAGES && g_ChatSeen < newest; page++) {
			q.afterId = g_ChatSeen;
			const auto messages = Database::Get()->GetChatMessages(q);
			if (messages.empty()) break;
			for (const auto& message : messages) {
				g_ChatMessages[message.channel]++;
				if (message.blocked) g_ChatBlocked[message.channel]++;
				g_ChatSeen = std::max(g_ChatSeen, message.id);
			}
		}
		// A flood bigger than the pages above is skipped rather than read on every refresh after
		g_ChatSeen = std::max(g_ChatSeen, newest);
	}

	// A database step, timed; a failure is logged and leaves its metrics out
	bool Timed(MetricsFormat::Writer& w, const std::string& query, const std::function<void()>& step) {
		const auto start = Clock::now();
		try {
			step();
		} catch (const std::exception& ex) {
			LOG_DEBUG("Metrics: %s failed: %s", query.c_str(), ex.what());
			return false;
		}
		w.Add("darkflame_db_query_seconds", "How long each of the database reads behind these metrics took on the last refresh", "gauge",
			{ {"query", query} }, std::chrono::duration<double>(Clock::now() - start).count());
		return true;
	}

	void AddDatabase(MetricsFormat::Writer& w) {
		bool ok = true;
		auto* db = Database::Get();
		w.Declare("darkflame_moderation_queue", "Things waiting for staff: names, pet names and properties to approve, bug reports, economy flags, player reports", "gauge");

		ok &= Timed(w, "snapshot", [&] {
			const auto snapshot = db->GetDashboardSnapshot();
			w.Add("darkflame_accounts", "Accounts", "gauge", static_cast<double>(snapshot.accounts));
			w.Add("darkflame_characters", "Characters", "gauge", static_cast<double>(snapshot.characters));
			w.Add("darkflame_properties", "Properties", "gauge", static_cast<double>(snapshot.properties));
			const auto queue = [&](const char* name, uint64_t size) {
				w.Add("darkflame_moderation_queue", "", "gauge", { {"queue", name} }, static_cast<double>(size));
			};
			queue("names", snapshot.pendingNames);
			queue("pet_names", snapshot.pendingPetNames);
			queue("properties", snapshot.pendingProperties);
			queue("bug_reports", snapshot.unresolvedBugReports);
			queue("economy_flags", snapshot.openEconomyFlags);
			CountChat(snapshot.chatLogMaxId);
		});
		for (const auto& [channel, count] : g_ChatMessages) {
			w.Add("darkflame_chat_messages_total", "Chat messages logged since the dashboard started, blocked ones included", "counter", { {"channel", channel} }, static_cast<double>(count));
		}
		for (const auto& [channel, count] : g_ChatBlocked) {
			w.Add("darkflame_chat_messages_blocked_total", "Chat messages the chat filter stopped since the dashboard started", "counter", { {"channel", channel} }, static_cast<double>(count));
		}

		ok &= Timed(w, "player_reports", [&] {
			IModeration::PlayerReportQuery q;
			q.status = static_cast<int16_t>(ePlayerReportStatus::OPEN);
			w.Add("darkflame_moderation_queue", "", "gauge", { {"queue", "player_reports"} }, db->CountPlayerReports(q));
		});

		// The UGC server's work list: models and modular builds by processing state
		ok &= Timed(w, "ugc", [&] {
			w.Declare("darkflame_ugc_items", "Player models (model) and cars and rockets (modular) by UGC processing state", "gauge");
			const auto add = [&](const char* kind, const std::vector<std::pair<IUgc::eProcessState, uint64_t>>& rows) {
				std::map<std::string, uint64_t> states;
				for (const auto state : magic_enum::enum_values<IUgc::eProcessState>()) states[IUgc::ProcessStateName(state)] = 0;
				for (const auto& [state, count] : rows) states[IUgc::ProcessStateName(state)] += count;
				for (const auto& [state, count] : states) w.Add("darkflame_ugc_items", "", "gauge", { {"kind", kind}, {"state", state} }, static_cast<double>(count));
			};
			add("model", db->GetUgcProcessCounts());
			add("modular", db->GetModularBuildProcessCounts());
		});

		ok &= Timed(w, "strikes", [&] {
			w.Add("darkflame_active_strikes", "Strikes that still count, all accounts together", "gauge", db->CountActiveStrikes(0, Strikes::CountsSince()));
		});

		// Today's ledger, as far as the world servers have written it (they send it in batches). Resets at midnight UTC.
		ok &= Timed(w, "economy", [&] {
			const auto today = static_cast<uint32_t>(std::time(nullptr) / (24 * 60 * 60));
			const auto sources = EconomySourceNames();
			const auto source = [&](const nlohmann::json& row) {
				const auto id = std::to_string(row.value("source", 0));
				return LowerName(sources.contains(id) ? sources[id].get<std::string>() : id);
			};
			w.Declare("darkflame_economy_coins_today", "Coins gained and spent today (UTC) by source, staff included", "gauge");
			for (const auto& row : db->GetCurrencyFlows(today, today, false)) {
				w.Add("darkflame_economy_coins_today", "", "gauge", { {"direction", "gained"}, {"source", source(row)} }, static_cast<double>(row.value("gained", int64_t{})));
				w.Add("darkflame_economy_coins_today", "", "gauge", { {"direction", "spent"}, {"source", source(row)} }, static_cast<double>(row.value("spent", int64_t{})));
			}
			w.Declare("darkflame_economy_items_today", "Items created and destroyed today (UTC) by source, staff included", "gauge");
			for (const auto& row : db->GetItemFlows(today, today, 0, false)) {
				w.Add("darkflame_economy_items_today", "", "gauge", { {"direction", "created"}, {"source", source(row)} }, static_cast<double>(row.value("created", int64_t{})));
				w.Add("darkflame_economy_items_today", "", "gauge", { {"direction", "destroyed"}, {"source", source(row)} }, static_cast<double>(row.value("destroyed", int64_t{})));
			}
			std::map<std::string, double> kinds;
			for (const auto kind : magic_enum::enum_values<IEconomyLedger::eMapEvent>()) kinds[LowerName(magic_enum::enum_name(kind))] = 0;
			for (const auto& row : db->GetMapEventsPerDay(today, today, {})) {
				const auto kind = magic_enum::enum_cast<IEconomyLedger::eMapEvent>(row.value("kind", 0));
				if (kind) kinds[LowerName(magic_enum::enum_name(*kind))] += static_cast<double>(row.value("events", int64_t{}));
			}
			for (const auto& [kind, events] : kinds) {
				w.Add("darkflame_map_events_today", "Map events today (UTC) by kind: kills, drops, deaths, smashes, quickbuilds", "gauge", { {"kind", kind} }, events);
			}
		});

		ok &= Timed(w, "tasks", [&] {
			w.Declare("darkflame_task_last_run_success", "Whether a scheduled task's last finished run succeeded", "gauge");
			for (const auto& run : db->GetLatestTaskRuns()) {
				const Labels labels{ {"task", run.task} };
				w.Add("darkflame_task_last_run_success", "", "gauge", labels, run.status == IScheduledTasks::eRunStatus::SUCCEEDED ? 1 : 0);
				w.Add("darkflame_task_last_run_timestamp_seconds", "When a scheduled task's last run finished (Unix time)", "gauge", labels, static_cast<double>(run.finishedAt));
				w.Add("darkflame_task_last_run_duration_seconds", "How long a scheduled task's last run took", "gauge", labels, static_cast<double>(std::max<int64_t>(0, run.finishedAt - run.startedAt)));
			}
		});

		w.Add("darkflame_db_up", "Whether every database read behind these metrics worked on the last refresh", "gauge", ok ? 1 : 0);
	}

	const std::string& Text() {
		const auto cacheSeconds = std::clamp<int64_t>(GeneralUtils::TryParse<int64_t>(Game::config->GetValue("metrics_cache_seconds")).value_or(10), 1, 300);
		const auto now = Clock::now();
		if (!g_Text.empty() && now - g_BuiltAt < std::chrono::seconds(cacheSeconds)) return g_Text;
		MetricsFormat::Writer w;
		AddLive(w);
		Traffic::AddMetrics(w);
		AddDatabase(w);
		g_Text = w.Text();
		g_BuiltAt = now;
		return g_Text;
	}

	void Send(HTTPReply& reply) {
		reply.status = eHTTPStatusCode::OK;
		reply.contentType = eContentType::TEXT_PROMETHEUS;
		reply.message = Text();
	}

	void PlainError(HTTPReply& reply, eHTTPStatusCode status, const std::string& message) {
		reply.status = status;
		reply.contentType = eContentType::TEXT_PLAIN;
		reply.message = message + "\n";
	}

	bool AddressAllowed(const HTTPContext& context) {
		return MetricsFormat::AddressAllowed(Game::config->GetValue("metrics_allowed_ips"), ClientAddress(context));
	}

	// The shared scraper secret, as Authorization: Bearer <metrics_token>
	bool SharedTokenGiven(const HTTPContext& context) {
		const auto secret = Game::config->GetValue("metrics_token");
		const auto& header = context.GetHeader("Authorization");
		if (secret.size() < MIN_TOKEN_LENGTH || !header.starts_with("Bearer ")) return false;
		return ConstantTimeEquals(std::string_view(header).substr(7), secret);
	}

	// A signed-in account (API token or the browser's session) that may see metrics
	bool AccountAllowed(const HTTPContext& context) {
		if (!context.isAuthenticated || context.userData.contains("needs_2fa")) return false;
		const auto source = context.userData.find("auth_source");
		if (source != context.userData.end() && source->second == "header" && !Permissions::Allowed(context.gmLevel, "api_access", nullptr, context.grants.get())) return false;
		return Permissions::Allowed(context.gmLevel, "metrics_view", context.apiKey.get(), context.grants.get());
	}
}

namespace PrometheusMetrics {
	void RegisterRoutes() {
		// Not behind the sign-in middleware: a scraper may use the shared token instead of an account's
		Route(eHTTPMethod::GET, "/metrics", PUBLIC, "Prometheus metrics, when metrics_enabled is on. Send Authorization: Bearer with an API token (metrics_view) or metrics_token",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!ConfigFlag("metrics_enabled", false)) return PlainError(reply, eHTTPStatusCode::NOT_FOUND, "Not found");
				const auto address = ClientAddress(context);
				if (!AddressAllowed(context)) return PlainError(reply, eHTTPStatusCode::FORBIDDEN, "Not allowed from this address");
				const auto now = Clock::now();
				if (const auto blocked = g_BlockedUntil.find(address); blocked != g_BlockedUntil.end()) {
					if (now < blocked->second) return PlainError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many refused requests; wait a minute");
					g_BlockedUntil.erase(blocked);
				}
				if (SharedTokenGiven(context) || AccountAllowed(context)) return Send(reply);

				if (!g_Refusals.Allow(address)) {
					LOG("Metrics: too many refused requests from %s", address.c_str());
					if (g_BlockedUntil.size() > 10000) g_BlockedUntil.clear();
					g_BlockedUntil[address] = now + std::chrono::seconds(60);
				}
				if (context.isAuthenticated) return PlainError(reply, eHTTPStatusCode::FORBIDDEN, "Your account may not see metrics (metrics_view)");
				reply.headers.push_back("WWW-Authenticate: Bearer realm=\"metrics\"");
				PlainError(reply, eHTTPStatusCode::UNAUTHORIZED, "Send Authorization: Bearer <API token or metrics_token>");
			});

		Route(eHTTPMethod::GET, "/api/metrics", Perm("metrics_view"),
			"The same Prometheus metrics as /metrics, for API tokens (text, not JSON). Off until metrics_enabled is on; metrics_allowed_ips applies too. "
			"Scrapers without an account can use /metrics with the shared metrics_token as Authorization: Bearer",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!ConfigFlag("metrics_enabled", false)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Metrics are switched off (metrics_enabled)");
				if (!AddressAllowed(context)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Not allowed from this address");
				Send(reply);
			});
	}
}
