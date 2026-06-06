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
	// Get server state data (auth, chat, worlds) - mutex is acquired internally
	nlohmann::json data = ServerState::GetServerStateJson();
	
	// Add statistics
	try {
		data["stats"]["onlinePlayers"] = 0; // TODO: Get from server communication
		data["stats"]["totalAccounts"] = Database::Get()->GetAccountCount();
		data["stats"]["totalCharacters"] = Database::Get()->GetCharacterCount();
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error getting stats: %s", ex.what());
	}
	
	// Broadcast to all connected WebSocket clients subscribed to "dashboard_update"
	Game::web.SendWSMessage("dashboard_update", data);
}
