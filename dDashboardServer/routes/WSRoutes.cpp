#include "WSRoutes.h"
#include "ChatRoutes.h"
#include "Permissions.h"
#include "ServerState.h"
#include "Web.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"
#include "master/DataChanged.h"
#include "Alerts.h"
#include "LiveWorld.h"
#include "GeneralUtils.h"

#include <set>

namespace {
	std::optional<IDashboardStats::Snapshot> g_LastSnapshot;
	nlohmann::json g_LastStatus;

	nlohmann::json ModerationCounts(const IDashboardStats::Snapshot& snapshot) {
		return {
			{"pendingNames", snapshot.pendingNames},
			{"pendingPetNames", snapshot.pendingPetNames},
			{"pendingProperties", snapshot.pendingProperties},
			{"unresolvedBugReports", snapshot.unresolvedBugReports},
			{"openEconomyFlags", snapshot.openEconomyFlags}
		};
	}

	// Emit table_changed for every table whose aggregate values moved since the last tick
	void DiffSnapshots(const IDashboardStats::Snapshot& previous, const IDashboardStats::Snapshot& current) {
		const auto changed = [&](const char* table, bool differs) { if (differs) BroadcastTableChanged(table); };
		changed("accounts", previous.accounts != current.accounts || previous.accountsMaxId != current.accountsMaxId);
		changed("characters", previous.characters != current.characters);
		changed("pending_names", previous.pendingNames != current.pendingNames);
		changed("properties", previous.properties != current.properties || previous.pendingProperties != current.pendingProperties);
		changed("play_keys", previous.playKeys != current.playKeys);
		changed("bug_reports", previous.bugReports != current.bugReports || previous.unresolvedBugReports != current.unresolvedBugReports);
		// Bug reports written by something other than a world server we heard from
		if (current.bugReports > previous.bugReports) Alerts::CheckNewBugReports();
		changed("pet_names", previous.petNames != current.petNames || previous.pendingPetNames != current.pendingPetNames);
		changed("activity_log", previous.activityLogMaxId != current.activityLogMaxId);
		changed("chat", previous.chatLogMaxId != current.chatLogMaxId);
		ChatRoutes::PushNew(current.chatLogMaxId);
		changed("command_log", previous.commandLogMaxId != current.commandLogMaxId);
		changed("audit_log", previous.auditLogMaxId != current.auditLogMaxId);
		changed("economy_flags", previous.openEconomyFlags != current.openEconomyFlags || previous.economyFlagsMaxId != current.economyFlagsMaxId);
	}
}

void RegisterWSRoutes() {
	Game::web.RegisterWSSubscription("dashboard_update", 0);
	Game::web.RegisterWSSubscription("table_changed", 1);
	Game::web.RegisterWSSubscription("moderation_counts", std::function<uint8_t()>([] { return Permissions::Level("moderate_names"); }), "moderate_names");
	// Delivered only to the account that started the action (Web::SendWSMessageToAccount), so players get their own
	Game::web.RegisterWSSubscription("action_result", 0);
}

void BroadcastDashboardUpdate() {
	// Sent to every signed-in account, so it only has what players may see; staff load /api/worlds for details
	nlohmann::json status = ServerState::PlayerSafe(ServerState::GetServerStateJson());
	Alerts::ServerStatus(status["auth"].value("online", false), status["chat"].value("online", false));
	status["restart"] = LiveWorld::RestartStatus();

	try {
		const auto snapshot = Database::Get()->GetDashboardSnapshot();
		status["stats"]["totalAccounts"] = snapshot.accounts;
		status["stats"]["totalCharacters"] = snapshot.characters;
		status["stats"]["totalProperties"] = snapshot.properties;
		status["stats"]["totalPlayKeys"] = snapshot.playKeys;

		if (!g_LastSnapshot || *g_LastSnapshot != snapshot) {
			if (g_LastSnapshot) DiffSnapshots(*g_LastSnapshot, snapshot);
			auto counts = ModerationCounts(snapshot);
			Game::web.SendWSMessage("moderation_counts", counts);
			g_LastSnapshot = snapshot;
		}
	} catch (const std::exception& ex) {
		LOG_DEBUG("Error getting dashboard snapshot: %s", ex.what());
	}

	// Only push status when something actually changed; clients fetch the initial state over HTTP
	if (status != g_LastStatus) {
		g_LastStatus = status;
		Game::web.SendWSMessage("dashboard_update", status);
	}
}

void BroadcastTableChanged(const std::string& table, const std::string& id) {
	// A banned, locked, demoted or signed-out account's open sockets are checked again right away
	if (table == "accounts") Game::web.RecheckWebSockets(id.empty() ? 0 : GeneralUtils::TryParse<uint32_t>(id).value_or(0));
	nlohmann::json message{ {"table", table} };
	if (!id.empty()) message["id"] = id;
	Game::web.SendWSMessage("table_changed", message);
}

void BroadcastDataChanged(const DataChanged& changed) {
	static const std::set<std::string> knownTables{
		"accounts", "characters", "pending_names", "pet_names", "properties", "bug_reports", "mail", "economy", "player_reports", "live_events", "challenges"
	};
	for (const auto& entry : changed.entries) {
		const auto id = entry.id != 0 ? std::to_string(entry.id) : "";
		// In-game bans and mutes change an account and are worth an alert
		if (entry.table == "account_banned" || entry.table == "account_muted") {
			BroadcastTableChanged("accounts", id);
			Alerts::InGameAccountAction(entry.table, static_cast<uint32_t>(entry.id));
			// Bans and mutes done with GM commands go in the account's history too
			Database::Get()->InsertAccountNote({ 0, static_cast<uint32_t>(entry.id), entry.table == "account_banned" ? "ban" : "mute",
				entry.table == "account_banned" ? "Banned with a GM command in game" : "Muted with a GM command in game", "[in game]", static_cast<int64_t>(std::time(nullptr)) });
			continue;
		}
		if (!knownTables.contains(entry.table)) continue;
		BroadcastTableChanged(entry.table, id);
		if (entry.table == "bug_reports") Alerts::CheckNewBugReports();
		if (entry.table == "pending_names" && entry.id != 0) Alerts::PendingName(entry.id);
	}
}
