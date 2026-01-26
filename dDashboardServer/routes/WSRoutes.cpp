#include "WSRoutes.h"
#include "ServerState.h"
#include "Web.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"

void RegisterWSRoutes() {
	// Register WebSocket subscriptions for real-time updates
	Game::web.RegisterWSSubscription("dashboard_update");
	Game::web.RegisterWSSubscription("server_status");
	Game::web.RegisterWSSubscription("player_joined");
	Game::web.RegisterWSSubscription("player_left");
	
	// dashboard_update: Broadcasts complete dashboard data every 2 seconds
	// Other subscriptions can be triggered by events from the master server
}

void BroadcastDashboardUpdate() {
	std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
	
	nlohmann::json data = {
		{"auth", {
			{"online", ServerState::g_AuthStatus.online},
			{"players", ServerState::g_AuthStatus.players},
			{"version", ServerState::g_AuthStatus.version}
		}},
		{"chat", {
			{"online", ServerState::g_ChatStatus.online},
			{"players", ServerState::g_ChatStatus.players}
		}},
		{"worlds", nlohmann::json::array()}
	};
	
	for (const auto& world : ServerState::g_WorldInstances) {
		data["worlds"].push_back({
			{"mapID", world.mapID},
			{"instanceID", world.instanceID},
			{"cloneID", world.cloneID},
			{"players", world.players},
			{"isPrivate", world.isPrivate}
		});
	}
	
	// Add statistics
	try {
		const uint32_t accountCount = Database::Get()->GetAccountCount();
		data["stats"]["onlinePlayers"] = 0; // TODO: Get from server communication
		data["stats"]["totalAccounts"] = accountCount;
		data["stats"]["totalCharacters"] = 0; // TODO: Add GetCharacterCount to database interface
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error getting stats: %s", ex.what());
	}
	
	// Broadcast to all connected WebSocket clients subscribed to "dashboard_update"
	Game::web.SendWSMessage("dashboard_update", data);
}
