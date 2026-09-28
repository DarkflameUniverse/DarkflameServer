#include "Permissions.h"
#include "AccountRules.h"
#include "ApiKeyScope.h"

#include <map>

#include "Game.h"
#include "dConfig.h"
#include "GeneralUtils.h"

namespace {
	// Defaults match what each GM level could do before permissions were configurable
	const std::vector<Permissions::Permission> PERMISSIONS{
		// What every account can do by default; raise these to keep players (or low staff levels) out
		{ "own_characters", "Players", "Their own characters", "See their own characters: inventory, missions, progress, mail, pets, friends and name requests", 0, false, Permissions::PLAYER_LEVEL },
		{ "own_properties", "Players", "Their own properties", "See their own properties, the 3D view and model files", 0, false, Permissions::PLAYER_LEVEL },
		{ "own_history", "Players", "Their trade and mail history", "See the trades and mail their characters were part of", 0, false, Permissions::PLAYER_LEVEL },
		{ "leaderboards_view", "Players", "Leaderboards", "The Leaderboards page", 0, false, Permissions::PLAYER_LEVEL },
		{ "showcase_view", "Players", "Property showcase", "Browse other players' approved public properties and walk around them in 3D (setting showcase_public opens it to everyone)", 0, false, Permissions::PLAYER_LEVEL },
		{ "own_strikes", "Players", "Their strikes", "See the strikes on their own account and why they were given", 0, false, Permissions::PLAYER_LEVEL },
		{ "challenges_view", "Players", "Community challenges", "See the community challenges, how far along they are and what their own characters added", 0, false, Permissions::PLAYER_LEVEL },
		{ "api_access", "Players", "API access", "Make API keys and use the dashboard's API with them. A key only does what its owner may, narrowed to the permissions chosen for it", 0, false, Permissions::PLAYER_LEVEL },

		{ "accounts_view", "Accounts", "View accounts", "The accounts list and other people's account pages", 1 },
		{ "accounts_notes", "Accounts", "Moderation history", "See and add notes and warnings on accounts (warnings can be sent to the player)", 2 },
		{ "accounts_kick", "Accounts", "Kick", "Disconnect an account's online sessions", 2 },
		{ "accounts_mute", "Accounts", "Mute", "Mute and unmute accounts", 2 },
		{ "accounts_ban", "Accounts", "Ban and lock", "Ban, unban, lock and unlock accounts", 4 },
		{ "accounts_links", "Accounts", "Linked accounts", "See other accounts that share a play key, email address or login address with an account", 3 },
		{ "accounts_send_reset", "Accounts", "Send password reset", "Email an account a password reset link", 4 },
		{ "accounts_manage", "Accounts", "Manage accounts", "Create accounts, change their email or password, reset their two-factor login", 8 },
		{ "accounts_gm_level", "Accounts", "Set GM levels", "Change GM levels (never to your own level or above, unless GM 9; their own only with self_moderation, and only lower)", 8 },
		{ "accounts_delete", "Accounts", "Delete accounts", "Permanently delete an account and its characters", 9 },
		{ "api_keys_manage", "Accounts", "Other accounts' API keys", "See and revoke other accounts' API keys (the rank rules apply; nobody can make keys for someone else)", 8 },
		// Who staff may use their tools on. Each needs the tool's own permission as well; GM 9 may always act on anyone,
		// and nobody below GM 9 can ever act on a higher GM level or raise their own
		{ "manage_equal_rank", "Accounts", "Act on their own rank", "Use their account and character tools on other accounts with the same GM level as their own (never on a higher one)", 9 },
		{ "self_tools", "Accounts", "Everyday tools on themselves", "Use tools they have on their own account and characters where nothing is gained: rescue or move their own characters, kick themselves, sign themselves out everywhere, email themselves a reset link", 1 },
		{ "self_items", "Accounts", "Give themselves items and progress", "Use tools they have on their own characters that give something: edit coins, U-score, level and items, restore versions, replace XML, change missions, give lost items back, and mail items to their own characters (also in mail to everyone)", 9 },
		{ "self_moderation", "Accounts", "Change their own record", "Use moderation and account tools they have on their own account: ban, mute, lock and lift them, give or revoke their own strikes, warnings, deleting history entries, restrictions on their own characters, lowering their own GM level, email, password or two-factor reset without the current one, deleting their own account", 9 },

		{ "characters_view", "Characters", "View characters", "The characters list and other players' characters, including their XML", 1 },
		{ "characters_mail", "Characters", "Read mailboxes", "See other players' mail", 3 },
		{ "characters_rescue", "Characters", "Rescue", "Move a stuck character to a zone's spawn point", 3 },
		{ "characters_restrict", "Characters", "Restrict", "Block a character's trading, mail or chat", 3 },
		{ "characters_history", "Characters", "Character history", "See, compare and download earlier versions of characters", 3 },
		{ "characters_edit", "Characters", "Edit characters", "Change coins, U-score, level and items, and restore earlier versions", 8 },
		{ "characters_edit_xml", "Characters", "Replace character XML", "Upload new character XML (also needs enable_char_xml_upload=1)", 8 },
		{ "characters_missions", "Characters", "Change missions", "Complete (with or without rewards), reset or give a character a mission, e.g. for a stuck player", 5 },

		{ "players_view", "Moderation", "See who is online", "The Online Players page and live player positions on the world map", 3 },
		{ "players_history", "Moderation", "Player movement history", "Replay where players went on the 3D world view (positions kept for position_history_days; every replay is audited)", 5 },
		{ "moderate_names", "Moderation", "Moderate names", "The moderation queue: approve and reject character names", 3 },
		{ "moderate_pet_names", "Moderation", "Moderate pet names", "Approve and reject pet names", 5 },
		{ "chat_view", "Moderation", "Read chat", "The chat log: what players said in zones, and what the chat filter stopped", 3 },
		{ "chat_private", "Moderation", "Read private chat", "Whispers and team chat in the chat log", 8 },
		{ "chat_filter_manage", "Moderation", "Chat filter", "See and change the words the chat filter allows and stops, and apply them in running worlds", 5 },
		{ "player_reports_view", "Moderation", "View player reports", "Read what players reported from the game's Report Abuse window", 2 },
		{ "player_reports_manage", "Moderation", "Handle player reports", "Act on or dismiss player reports (a strike also needs strikes_give)", 3 },
		{ "leaderboards_manage", "Moderation", "Moderate leaderboards", "Remove scores (e.g. cheated ones) and clear whole leaderboards", 5 },
		{ "moderate_properties", "Moderation", "Moderate properties", "Approve and reject public properties", 5 },
		{ "feature_properties", "Moderation", "Top properties", "Choose which approved public property each slot of the game's \"Today's Top Properties\" news panel shows", 5 },
		{ "strikes_give", "Moderation", "Give strikes", "Add a strike to an account when rejecting a name, pet name or property or removing a score, or by hand on the account", 3 },
		{ "strikes_revoke", "Moderation", "Revoke strikes", "Take back a strike; it stays on record but stops counting", 5 },
		{ "ai_suggest", "Moderation", "AI moderator helper", "Ask the AI helper (when switched on in Settings) to draft a suggestion for a report, chat message, name or economy flag. Only a draft for staff: nothing is applied or shown to players unless you do it", 3 },
		{ "properties_view", "Moderation", "View properties", "The properties list and other players' properties and models", 1 },
		{ "properties_import", "Moderation", "Import models", "Place LXFML models onto a property", 8 },
		{ "bug_reports_view", "Moderation", "View bug reports", "Read bug reports", 1 },
		{ "bug_reports_manage", "Moderation", "Handle bug reports", "Resolve and delete bug reports", 3 },

		{ "mail_send", "Communication", "Send mail", "Send in-game mail to a player or everyone", 3 },
		{ "mail_items", "Communication", "Mail items", "Attach items to mail sent to one player (like /gmadditem; their own characters only with self_items)", 8 },
		{ "mail_broadcast_items", "Communication", "Mail items to everyone", "Attach items to mail sent to every character", 8 },
		{ "server_announce", "Communication", "Announcements", "Show an announcement to everyone online", 5 },
		{ "announcements_schedule", "Communication", "Scheduled announcements", "Set up, change and delete repeating in-game announcements, and add announcements to scheduled events", 5 },
		{ "worlds_manage", "Communication", "Shut down worlds", "Shut down one world instance (its players are disconnected)", 8 },
		{ "chat_send", "Communication", "Send chat into the game", "Post in players' chat from the dashboard or a chat bridge (e.g. a Discord bot); shown as [label] name", 8 },
		{ "server_restart", "Communication", "Scheduled restarts", "Schedule and cancel server restarts, also as part of scheduled events", 8 },

		{ "logs_activity", "Logs", "Activity log", "Zone enter and exit history", 1 },
		{ "logs_command", "Logs", "Command log", "Slash commands players used", 1 },
		{ "logs_audit", "Logs", "Audit log", "What staff did on the dashboard", 8 },
		{ "logs_system", "Logs", "Server logs", "Server log files, log search and crash dumps", 8 },
		{ "health_view", "Logs", "Server health", "Player counts, running worlds, uptime and memory over time", 8 },
		{ "metrics_view", "Logs", "Prometheus metrics", "Read /metrics (and /api/metrics) with an API token, when metrics_enabled is on", 8 },

		{ "reports_view", "Economy and map", "Economy reports", "Coins, U-score, items, trades and mail, item traces, duplicate scans, flags and the world map", 3 },
		{ "reports_review_flags", "Economy and map", "Review flags", "Mark economy flags dismissed or actioned", 3 },
		{ "items_restore", "Economy and map", "Give items back", "Mail a traced item back to a player, or what a character lost since one of its snapshots (with original IDs when they no longer exist)", 8 },
		{ "reports_run_checks", "Economy and map", "Run economy checks", "Run the anomaly checks by hand", 8 },
		{ "property_rent_manage", "Economy and map", "Property rent", "Change the rent of property worlds on the Property Rent page", 8 },
		{ "contraband_manage", "Economy and map", "Contraband list", "Add, change and remove contraband items (flagged, or removed from players, when a character has one)", 8 },

		{ "play_keys_manage", "Server", "Play keys", "Create, edit and delete play keys, and see the key an account used", 8 },
		{ "ugc_manage", "Server", "UGC processing", "Have the UGC server make player models' meshes and icons again", 8 },
		{ "client_files", "Server", "Client files", "Browse and download the game client's files", 8 },
		{ "vanity_manage", "Server", "Vanity NPCs", "Edit the vanity files, NPCs and plaque texts, add vanity changes to scheduled events, and respawn them in game", 8 },
		{ "tasks_view", "Server", "View scheduled tasks", "See scheduled tasks, their runs and logs", 8 },
		{ "tasks_manage", "Server", "Manage scheduled tasks", "Change schedules, switch tasks on and off, run them now", 9 },
		{ "live_events_manage", "Server", "Live events", "Start and end live events in game: treasure hunts, bonus coins/U-score/loot, invasions and celebrations; also as part of scheduled events", 8 },
		{ "challenges_manage", "Server", "Community challenges", "Create, change, cancel and delete server-wide challenges and their rewards (rewards are mailed to everyone who took part)", 8 },
		{ "events_manage", "Server", "Scheduled events: features", "Scheduled events that switch game features (event_1..event_8) on and off", 8 },
		{ "server_live_update", "Server", "Live updates", "Move every server and world instance onto a new build without a restart (players see a short loading screen), and cancel one", 9 },
		{ "instances_manage", "Server", "Instance limits", "Change players per instance and spare instances per zone (the master server applies them)", 9 },
		{ "maintenance", "Server", "Data maintenance", "Repair tools on the Maintenance page", 9 },
		{ "backups", "Server", "Database backups", "Make, download and delete database backups (downloads also need your password)", 9 },
		{ "webhooks", "Server", "Webhooks", "Add and change outgoing alert webhooks", 9 },
		{ "email_settings", "Server", "Email settings", "Connect the mail account and send test emails", 9 },
		{ "settings", "Server", "Server settings", "Change any server setting on the Settings page (always GM 9)", 9, true },
		{ "permissions_manage", "Server", "Permissions", "Change what each GM level may do (always GM 9)", 9, true },

		{ "dev_message_inspector", "Developer tools", "Game message inspector", "Capture the game messages an online player sends and receives, live (every capture is audited)", 8 },
		{ "dev_cdclient", "Developer tools", "CDClient browser", "Browse the game's CDClient data: objects and their components, loot, missions, skills, behaviors and activities", 8 },
	};

	const std::map<std::string, const Permissions::Permission*>& Index() {
		static const auto index = [] {
			std::map<std::string, const Permissions::Permission*> map;
			for (const auto& permission : PERMISSIONS) map[permission.key] = &permission;
			return map;
		}();
		return index;
	}
}

namespace Permissions {
	const std::vector<Permission>& All() {
		return PERMISSIONS;
	}

	const Permission* Find(const std::string& key) {
		const auto it = Index().find(key);
		return it == Index().end() ? nullptr : it->second;
	}

	std::string ConfigName(const std::string& key) {
		return std::string(PREFIX) + key;
	}

	uint8_t Resolve(const Permission& permission, const std::string& configValue) {
		if (permission.locked || configValue.empty()) return permission.defaultLevel;
		const auto parsed = GeneralUtils::TryParse<int32_t>(configValue);
		if (!parsed || *parsed < permission.minLevel || *parsed > MAX_LEVEL) return permission.defaultLevel;
		return static_cast<uint8_t>(*parsed);
	}

	uint8_t Level(const std::string& key) {
		const auto* permission = Find(key);
		if (!permission) return MAX_LEVEL + 1;
		return Resolve(*permission, Game::config ? Game::config->GetValue(ConfigName(key)) : "");
	}

	bool Allowed(uint8_t gmLevel, const std::string& key) {
		return gmLevel >= Level(key);
	}

	bool Allowed(uint8_t gmLevel, const std::string& key, const ApiKeys::Scope* scope) {
		return Allowed(gmLevel, key) && (!scope || scope->Has(key));
	}

	std::set<std::string> NotGrantable(uint8_t gmLevel, const std::set<std::string>& requested) {
		std::set<std::string> refused;
		for (const auto& permission : requested) {
			if (!Find(permission) || !Allowed(gmLevel, permission)) refused.insert(permission);
		}
		return refused;
	}

	bool CanViewCharacter(uint8_t gmLevel, uint32_t viewerAccountId, uint32_t ownerAccountId, const ApiKeys::Scope* scope) {
		const bool own = viewerAccountId != 0 && viewerAccountId == ownerAccountId;
		return Allowed(gmLevel, "characters_view", scope) || (own && Allowed(gmLevel, "own_characters", scope));
	}

	nlohmann::json ForLevel(uint8_t gmLevel, const ApiKeys::Scope* scope) {
		nlohmann::json can = nlohmann::json::object();
		for (const auto& permission : PERMISSIONS) can[permission.key] = Allowed(gmLevel, permission.key, scope);
		return can;
	}
}

namespace AccountRules {
	eManageDenial ManageDenialNow(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, eAccountAction action) {
		return ManageDenial(actorLevel, actorAccountId, targetLevel, targetAccountId,
			Permissions::Allowed(actorLevel, SelfPermission(action)), Permissions::Allowed(actorLevel, EQUAL_RANK_PERMISSION));
	}

	eManageDenial ManageDenialNow(uint8_t actorLevel, uint32_t actorAccountId, uint8_t targetLevel, uint32_t targetAccountId, eAccountAction action, const ApiKeys::Scope* scope) {
		const auto denial = ManageDenialNow(actorLevel, actorAccountId, targetLevel, targetAccountId, action);
		if (!scope) return denial;
		return ScopedManageDenial(denial, actorLevel, actorAccountId, targetLevel, targetAccountId,
			scope->Has(SelfPermission(action)), scope->Has(EQUAL_RANK_PERMISSION));
	}
}
