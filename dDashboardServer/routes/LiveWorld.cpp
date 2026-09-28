#include "LiveWorld.h"
#include "MasterPackets.h"
#include "WorldView.h"
#include "Permissions.h"
#include "DashboardRoutes.h"

#include <chrono>
#include <ctime>
#include <map>

#include "RouteUtils.h"
#include "Alerts.h"
#include "BitStreamUtils.h"
#include "ServerState.h"
#include "master/DashboardMessages.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "Web.h"
#include "dServer.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	using Clock = std::chrono::steady_clock;
	constexpr auto PUSH_INTERVAL = std::chrono::milliseconds(500);
	constexpr auto STALE_AFTER = std::chrono::seconds(20);
	constexpr const char* RESTART_STATE = "scheduled_restart";
	// Warnings before a scheduled restart, in seconds; only those shorter than the notice given are sent
	constexpr int64_t WARNINGS[] = { 3600, 1800, 900, 600, 300, 120, 60, 30, 10 };
	// How soon a scheduled restart may be (short ones are for quick restarts while nobody is on), and how late
	constexpr int64_t MIN_RESTART_SECONDS = 30;
	constexpr int64_t MAX_RESTART_SECONDS = 24 * 3600;

	struct WorldPlayers {
		PlayerPositions positions;
		Clock::time_point received;
	};
	std::map<std::pair<uint32_t, uint32_t>, WorldPlayers> g_Worlds; // by (zone, instance)
	std::map<LWOOBJID, std::string> g_Names;
	bool g_PositionsChanged = false;
	Clock::time_point g_NextPush{};

	struct Restart {
		int64_t at{};
		std::string reason;
		std::string by;
		int64_t lastWarning{}; // seconds-before of the last warning sent
	};
	std::optional<Restart> g_Restart;

	bool g_RestartLoaded = false;

	std::string CharacterName(LWOOBJID id) {
		const auto it = g_Names.find(id);
		if (it != g_Names.end()) return it->second;
		const auto info = Database::Get()->GetCharacterInfo(id);
		return g_Names[id] = info ? info->name : std::to_string(id);
	}

	nlohmann::json PlayersJson() {
		nlohmann::json players = nlohmann::json::array();
		for (const auto& [key, world] : g_Worlds) {
			for (const auto& player : world.positions.players) {
				players.push_back({
					{"id", std::to_string(player.characterId)}, {"name", CharacterName(player.characterId)},
					{"zone", world.positions.zoneId}, {"instance", world.positions.instanceId}, {"clone", world.positions.cloneId},
					{"x", player.x}, {"y", player.y}, {"z", player.z}
				});
			}
		}
		return players;
	}

	void SendAnnouncement(const std::string& title, const std::string& message, const std::vector<uint32_t>& zones = {}) {
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return;
		Announcement announcement;
		announcement.title = title;
		announcement.message = message;
		announcement.zones = zones;
		MasterPackets::SendToMaster(announcement);
	}

	std::string Duration(int64_t seconds) {
		if (seconds >= 3600 && seconds % 3600 == 0) return std::to_string(seconds / 3600) + (seconds == 3600 ? " hour" : " hours");
		if (seconds >= 60) {
			const auto minutes = (seconds + 30) / 60;
			return std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes");
		}
		return std::to_string(seconds) + " seconds";
	}

	void SaveRestart() {
		if (g_Restart) {
			Database::Get()->SetDashboardState(RESTART_STATE, nlohmann::json{
				{"at", g_Restart->at}, {"reason", g_Restart->reason}, {"by", g_Restart->by}, {"lastWarning", g_Restart->lastWarning}
			}.dump());
		} else {
			Database::Get()->DeleteDashboardState(RESTART_STATE);
		}
	}

	void LoadRestart() {
		g_RestartLoaded = true;
		const auto stored = Database::Get()->GetDashboardState(RESTART_STATE);
		if (!stored) return;
		const auto json = nlohmann::json::parse(*stored, nullptr, false);
		if (json.is_discarded()) return;
		g_Restart = Restart{ json.value("at", int64_t{ 0 }), json.value("reason", ""), json.value("by", ""), json.value("lastWarning", int64_t{ 0 }) };
		// A restart that was due while the dashboard was down is dropped rather than run by surprise
		if (g_Restart->at < std::time(nullptr)) {
			LOG("Dropping a scheduled restart that was due while the dashboard was offline");
			g_Restart.reset();
			SaveRestart();
		}
	}

	std::string RestartMessage(int64_t secondsLeft) {
		std::string message = "The server restarts in " + Duration(secondsLeft) + ".";
		if (!g_Restart->reason.empty()) message += " " + g_Restart->reason;
		return message;
	}

	void TickRestart() {
		if (!g_RestartLoaded) LoadRestart();
		if (!g_Restart) return;
		const auto left = g_Restart->at - static_cast<int64_t>(std::time(nullptr));
		if (left <= 0) {
			LOG("Scheduled restart: shutting the server down now");
			SendAnnouncement("Server restarting", "The server is restarting now. Please log back in in a few minutes.");
			g_Restart.reset();
			SaveRestart();
			if (Game::server && Game::server->GetIsConnectedToMaster()) {
				MasterPackets::SendToMaster(MasterPackets::DashboardShutdown());
			}
			return;
		}
		for (const auto warning : WARNINGS) {
			if (left > warning) continue;
			// The largest threshold we are inside of, sent once
			if (g_Restart->lastWarning == 0 || warning < g_Restart->lastWarning) {
				g_Restart->lastWarning = warning;
				SaveRestart();
				SendAnnouncement("Server restart", RestartMessage(left));
			}
			break;
		}
	}
}

namespace LiveWorld {
	std::optional<std::string> ScheduleRestart(int64_t seconds, const std::string& reason, const HTTPContext& actor, bool replace) {
		if (seconds < MIN_RESTART_SECONDS || seconds > MAX_RESTART_SECONDS) return "Pick between 30 seconds and 24 hours";
		if (reason.size() > 300) return "The reason can be at most 300 characters";
		if (!g_RestartLoaded) LoadRestart();
		if (g_Restart && !replace) return "A restart is already scheduled (by " + g_Restart->by + ")";

		g_Restart = Restart{ static_cast<int64_t>(std::time(nullptr)) + seconds, reason, actor.authenticatedUser, 0 };
		SaveRestart();
		g_NextPush = {}; // send the first warning right away
		Audit(actor, "schedule_restart", "In " + Duration(seconds) + (reason.empty() ? "" : ": " + reason));
		Alerts::Emit("server", "Restart scheduled", "The server restarts in " + Duration(seconds) + ".",
			{ { "By", actor.authenticatedUser }, { "Reason", reason } }, "/");
		return std::nullopt;
	}

	void HandlePlayerPositions(const PlayerPositions& positions) {
		const std::pair key{ positions.zoneId, positions.instanceId };
		if (positions.players.empty()) g_Worlds.erase(key);
		else g_Worlds[key] = { positions, Clock::now() };
		g_PositionsChanged = true;
		WorldView::RecordPositions(positions);
	}

	void Update() {
		const auto now = Clock::now();
		if (now < g_NextPush) return;
		g_NextPush = now + PUSH_INTERVAL;

		try {
			TickRestart();
		} catch (const std::exception& ex) {
			LOG("Scheduled restart check failed: %s", ex.what());
		}

		// Worlds that crashed stop reporting; forget them after a while
		const auto before = g_Worlds.size();
		std::erase_if(g_Worlds, [now](const auto& entry) { return now - entry.second.received > STALE_AFTER; });
		if (g_Worlds.size() != before) g_PositionsChanged = true;
		if (!g_PositionsChanged) return;
		g_PositionsChanged = false;
		nlohmann::json message{ {"players", PlayersJson()} };
		Game::web.SendWSMessage("player_positions", message);
	}

	nlohmann::json OnlinePlayers() {
		return PlayersJson();
	}

	bool Announce(const std::string& title, const std::string& message, const std::vector<uint32_t>& zones) {
		if (!Game::server || !Game::server->GetIsConnectedToMaster()) return false;
		SendAnnouncement(title, message, zones);
		return true;
	}

	nlohmann::json RestartStatus(bool withStaffName) {
		if (!g_Restart) return nlohmann::json::object();
		nlohmann::json status{ {"at", g_Restart->at}, {"reason", g_Restart->reason} };
		if (withStaffName) status["by"] = g_Restart->by;
		return status;
	}

	void RegisterRoutes() {
		Game::web.RegisterWSSubscription("player_positions", std::function<uint8_t()>([] { return Permissions::Level("players_view"); }), "players_view");

		Route(eHTTPMethod::GET, "/api/live/players", Perm("players_view"), "Where online players are right now (also pushed on the player_positions socket topic)",
			[](HTTPReply& reply, const HTTPContext&) {
				JsonReply(reply, eHTTPStatusCode::OK, { {"players", PlayersJson()} });
			});

		Route(eHTTPMethod::POST, "/api/worlds/shutdown", Perm("worlds_manage"),
			"Shut down one world instance; its players are saved and disconnected, and a new instance starts when someone goes there. Body: {zone, instance}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const uint32_t zone = body->value("zone", 0u), instance = body->value("instance", 0u);
				bool running = false;
				uint32_t players = 0;
				{
					std::lock_guard lock(ServerState::g_StatusMutex);
					for (const auto& world : ServerState::g_WorldInstances) {
						if (world.mapID == zone && world.instanceID == instance) { running = true; players = world.players; }
					}
				}
				if (!running) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "That world instance isn't running");
				if (!Game::server || !Game::server->GetIsConnectedToMaster()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				MasterPackets::InstanceShutdown request;
				request.zoneID = zone;
				request.instanceID = instance;
				MasterPackets::SendToMaster(request);
				Audit(context, "shutdown_instance", "Zone " + std::to_string(zone) + " instance " + std::to_string(instance) + " (" + std::to_string(players) + " player(s))");
				JsonSuccess(reply, { {"message", "Shutting it down"} });
			});

		Route(eHTTPMethod::GET, "/api/worlds", Perm("players_view"), "Every world with its instance and clone IDs, address, players and, for property instances, the property and owner; worlds still starting or shutting down have state: starting|stopping",
			[](HTTPReply& reply, const HTTPContext&) {
				nlohmann::json worlds = nlohmann::json::array();
				std::lock_guard lock(ServerState::g_StatusMutex);
				for (const auto& w : ServerState::g_WorldInstances) {
					worlds.push_back({ {"mapID", w.mapID}, {"zoneName", w.zoneName}, {"instanceID", w.instanceID}, {"cloneID", w.cloneID}, {"players", w.players},
						{"isPrivate", w.isPrivate}, {"ip", w.ip}, {"port", w.port}, {"propertyId", w.propertyId}, {"propertyName", w.propertyName},
						{"ownerId", w.ownerId}, {"ownerName", w.ownerName} });
				}
				// Launched but not connected yet, or shutting down
				for (const auto& w : ServerState::g_PendingWorlds) {
					worlds.push_back({ {"mapID", w.mapID}, {"zoneName", w.zoneName}, {"instanceID", w.instanceID}, {"cloneID", w.cloneID}, {"players", 0},
						{"isPrivate", w.isPrivate}, {"ip", w.ip}, {"port", w.port}, {"propertyId", w.propertyId}, {"propertyName", w.propertyName},
						{"ownerId", w.ownerId}, {"ownerName", w.ownerName}, {"state", w.state} });
				}
				JsonSuccess(reply, { {"worlds", worlds} });
			});

		Route(eHTTPMethod::GET, "/api/players/online", Perm("players_view"), "Everyone online: character, account, GM level, world and position",
			[](HTTPReply& reply, const HTTPContext&) {
				auto players = PlayersJson();
				const auto& zones = ZoneNames();
				for (auto& player : players) {
					const auto zone = std::to_string(player.value("zone", 0));
					player["zone_name"] = zones.contains(zone) ? zones[zone].get<std::string>() : "Zone " + zone;
					const auto id = GeneralUtils::TryParse<LWOOBJID>(player.value("id", std::string{})).value_or(0);
					if (const auto info = Database::Get()->GetCharacterInfo(id)) {
						player["account_id"] = info->accountId;
						const auto account = Database::Get()->GetAccountById(info->accountId);
						player["account_name"] = account.value("name", std::string{});
						player["gm_level"] = account.value("gm_level", 0);
					}
				}
				JsonSuccess(reply, { {"players", players} });
			});

		Route(eHTTPMethod::POST, "/api/server/announce", Perm("server_announce"), "Show an announcement to every player online. Body: {title, message}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string title = body->value("title", "");
				const std::string message = body->value("message", "");
				if (message.empty() || message.size() > Announcement::MAX_MESSAGE || title.size() > Announcement::MAX_TITLE) {
					return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Write a message (up to 1000 characters) and a title up to 100");
				}
				if (!Game::server || !Game::server->GetIsConnectedToMaster()) return JsonError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "Not connected to the master server");
				SendAnnouncement(title, message);
				Audit(context, "announce", (title.empty() ? "" : title + ": ") + message);
				JsonSuccess(reply, { {"message", "Announcement sent"} });
			});

		Route(eHTTPMethod::GET, "/api/server/restart", Perm("server_restart"), "The scheduled restart, if any, with who scheduled it (everyone gets when and why in /api/status)",
			[](HTTPReply& reply, const HTTPContext&) {
				JsonReply(reply, eHTTPStatusCode::OK, RestartStatus(true));
			});

		Route(eHTTPMethod::POST, "/api/server/restart", Perm("server_restart"), "Schedule a restart with in-game warnings. Body: {seconds (30-86400) or minutes (1-1440), reason}. Needs a process supervisor to start the server again",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const int64_t seconds = body->contains("seconds") ? body->value("seconds", int64_t{ 0 }) : body->value("minutes", int64_t{ 0 }) * 60;
				const std::string reason = body->value("reason", "");
				if (const auto error = LiveWorld::ScheduleRestart(seconds, reason, context, true)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				JsonSuccess(reply, { {"message", "Restart scheduled"}, {"restart", RestartStatus(true)} });
			});

		Route(eHTTPMethod::POST, "/api/server/restart/cancel", Perm("server_restart"), "Cancel the scheduled restart",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!g_RestartLoaded) LoadRestart();
				if (!g_Restart) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No restart is scheduled");
				const bool warned = g_Restart->lastWarning != 0;
				g_Restart.reset();
				SaveRestart();
				if (warned) SendAnnouncement("Restart cancelled", "The scheduled server restart has been cancelled.");
				Audit(context, "cancel_restart", std::string("Cancelled the scheduled server restart") + (warned ? "; players were told" : ""));
				Alerts::Emit("server", "Restart cancelled", "The scheduled restart was cancelled.", { { "By", context.authenticatedUser } }, "/");
				JsonSuccess(reply, { {"message", "Restart cancelled"} });
			});
	}
}
