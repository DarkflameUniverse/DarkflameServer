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
	
	// Account subscriptions
	Game::web.RegisterWSSubscription("account_update");
	Game::web.RegisterWSSubscription("accounts_table_update");
	Game::web.RegisterWSSubscription("account_list_changed");
	
	// Character subscriptions
	Game::web.RegisterWSSubscription("character_update");
	Game::web.RegisterWSSubscription("characters_table_update");
	Game::web.RegisterWSSubscription("character_list_changed");
	
	// Property subscriptions
	Game::web.RegisterWSSubscription("property_update");
	Game::web.RegisterWSSubscription("properties_table_update");
	Game::web.RegisterWSSubscription("property_list_changed");
	
	// Play Key subscriptions
	Game::web.RegisterWSSubscription("play_key_update");
	Game::web.RegisterWSSubscription("play_keys_table_update");
	Game::web.RegisterWSSubscription("play_key_list_changed");
	
	// Bug Report subscriptions
	Game::web.RegisterWSSubscription("bug_report_update");
	Game::web.RegisterWSSubscription("bug_reports_table_update");
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

void BroadcastAccountUpdate(uint32_t accountId) {
	try {
		// Get updated account data
		nlohmann::json accountData = Database::Get()->GetAccountById(accountId);
		
		// Only broadcast if account was found
		if (!accountData.contains("error")) {
			accountData["event"] = "account_update";
			accountData["accountId"] = accountId;
			Game::web.SendWSMessage("account_update", accountData);
		}
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting account update: %s", ex.what());
	}
}

void BroadcastAccountsTableUpdate() {
	try {
		// Broadcast that the accounts table has been modified
		// Clients should reload the current page of data
		nlohmann::json message = {
			{"event", "accounts_table_update"},
			{"message", "Accounts table has been updated"}
		};
		Game::web.SendWSMessage("accounts_table_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting accounts table update: %s", ex.what());
	}
}

void BroadcastAccountListChanged() {
	try {
		// Broadcast that accounts list changed (new account created or deleted)
		nlohmann::json message = {
			{"event", "account_list_changed"},
			{"totalAccounts", Database::Get()->GetAccountCount()}
		};
		Game::web.SendWSMessage("account_list_changed", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting account list change: %s", ex.what());
	}
}

void BroadcastCharacterUpdate(uint32_t characterId) {
	try {
		// Get updated character data and broadcast
		nlohmann::json message = {
			{"event", "character_update"},
			{"characterId", characterId}
		};
		Game::web.SendWSMessage("character_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting character update: %s", ex.what());
	}
}

void BroadcastCharactersTableUpdate() {
	try {
		nlohmann::json message = {
			{"event", "characters_table_update"},
			{"message", "Characters table has been updated"}
		};
		Game::web.SendWSMessage("characters_table_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting characters table update: %s", ex.what());
	}
}

void BroadcastCharacterListChanged() {
	try {
		nlohmann::json message = {
			{"event", "character_list_changed"},
			{"totalCharacters", Database::Get()->GetCharacterCount()}
		};
		Game::web.SendWSMessage("character_list_changed", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting character list change: %s", ex.what());
	}
}

void BroadcastPropertyUpdate(uint32_t propertyId) {
	try {
		nlohmann::json message = {
			{"event", "property_update"},
			{"propertyId", propertyId}
		};
		Game::web.SendWSMessage("property_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting property update: %s", ex.what());
	}
}

void BroadcastPropertiesTableUpdate() {
	try {
		nlohmann::json message = {
			{"event", "properties_table_update"},
			{"message", "Properties table has been updated"}
		};
		Game::web.SendWSMessage("properties_table_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting properties table update: %s", ex.what());
	}
}

void BroadcastPropertyListChanged() {
	try {
		nlohmann::json message = {
			{"event", "property_list_changed"},
			{"totalProperties", 0} // TODO: Get from database
		};
		Game::web.SendWSMessage("property_list_changed", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting property list change: %s", ex.what());
	}
}

void BroadcastPlayKeyUpdate(uint32_t playKeyId) {
	try {
		nlohmann::json message = {
			{"event", "play_key_update"},
			{"playKeyId", playKeyId}
		};
		Game::web.SendWSMessage("play_key_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting play key update: %s", ex.what());
	}
}

void BroadcastPlayKeysTableUpdate() {
	try {
		nlohmann::json message = {
			{"event", "play_keys_table_update"},
			{"message", "Play Keys table has been updated"}
		};
		Game::web.SendWSMessage("play_keys_table_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting play keys table update: %s", ex.what());
	}
}

void BroadcastPlayKeyListChanged() {
	try {
		nlohmann::json message = {
			{"event", "play_key_list_changed"},
			{"totalPlayKeys", 0} // TODO: Get from database
		};
		Game::web.SendWSMessage("play_key_list_changed", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting play key list change: %s", ex.what());
	}
}

void BroadcastBugReportUpdate(uint32_t bugReportId) {
	try {
		nlohmann::json message = {
			{"event", "bug_report_update"},
			{"bugReportId", bugReportId}
		};
		Game::web.SendWSMessage("bug_report_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting bug report update: %s", ex.what());
	}
}

void BroadcastBugReportsTableUpdate() {
	try {
		nlohmann::json message = {
			{"event", "bug_reports_table_update"},
			{"message", "Bug Reports table has been updated"}
		};
		Game::web.SendWSMessage("bug_reports_table_update", message);
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error broadcasting bug reports table update: %s", ex.what());
	}
}
