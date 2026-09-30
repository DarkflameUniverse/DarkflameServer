#include "SettingsCatalog.h"

#include "UgcIconParams.h"

#include <charconv>
#include <cmath>
#include <map>
#include <sstream>

#include "GeneralUtils.h"

namespace {
	using namespace SettingsCatalog;

	const std::string SHARED = "sharedconfig.ini";
	const std::string MASTER = "masterconfig.ini";
	const std::string AUTH = "authconfig.ini";
	const std::string CHAT = "chatconfig.ini";
	const std::string WORLD = "worldconfig.ini";
	const std::string DASHBOARD = "dashboardconfig.ini";
	const std::string UGC = "ugcconfig.ini";

	constexpr double PORT_MIN = 1;
	constexpr double PORT_MAX = 65535;
	constexpr double DAYS_MAX = 36500;

	// ---- building blocks; the section is filled in by Catalog::Add ----

	Setting Bool(const std::string& file, const std::string& key, const std::string& title, const std::string& description, bool on, bool restart = false) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::BOOL, .defaultValue = on ? "1" : "0", .restart = restart };
	}

	Setting Int(const std::string& file, const std::string& key, const std::string& title, const std::string& description,
		std::string def, std::optional<double> min, std::optional<double> max, bool restart = false) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::INT, .defaultValue = std::move(def), .min = min, .max = max, .restart = restart };
	}

	Setting Port(const std::string& file, const std::string& key, const std::string& title, const std::string& description, std::string def) {
		return Int(file, key, title, description, std::move(def), PORT_MIN, PORT_MAX, true);
	}

	Setting Days(const std::string& key, const std::string& title, const std::string& description, std::string def, double min = 0) {
		auto setting = Int(DASHBOARD, key, title, description, std::move(def), min, DAYS_MAX);
		setting.unit = "days";
		return setting;
	}

	Setting Text(const std::string& file, const std::string& key, const std::string& title, const std::string& description,
		std::string def = "", bool restart = false) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::TEXT, .defaultValue = std::move(def), .restart = restart };
	}

	Setting Secret(const std::string& file, const std::string& key, const std::string& title, const std::string& description, bool restart = false) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::SECRET, .restart = restart };
	}

	Setting List(const std::string& file, const std::string& key, const std::string& title, const std::string& description, eListOf of, std::string def = "") {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::INT_LIST, .defaultValue = std::move(def), .listOf = of };
	}

	Setting Choice(const std::string& file, const std::string& key, const std::string& title, const std::string& description,
		std::string def, std::vector<std::string> choices, bool restart = false) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::CHOICE, .defaultValue = std::move(def), .choices = std::move(choices), .restart = restart };
	}

	Setting Float(const std::string& file, const std::string& key, const std::string& title, const std::string& description,
		std::string def, std::optional<double> min, std::optional<double> max) {
		return { .key = key, .file = file, .title = title, .description = description, .type = eType::FLOAT, .defaultValue = std::move(def), .min = min, .max = max };
	}

	// ---- modifiers ----

	Setting Unit(Setting setting, std::string unit) { setting.unit = std::move(unit); return setting; }
	Setting Labels(Setting setting, std::vector<std::string> labels) { setting.choiceLabels = std::move(labels); return setting; }
	Setting Format(Setting setting, eFormat format) { setting.format = format; return setting; }
	Setting Unused(Setting setting) { setting.unused = true; return setting; }
	Setting Multiline(Setting setting) { setting.multiline = true; return setting; }
	Setting When(Setting setting, const std::string& file, const std::string& key, std::vector<std::string> values) {
		setting.condition = Condition{ file, key, std::move(values) };
		return setting;
	}

	struct Catalog {
		std::vector<Category> categories;
		std::vector<Section> sections;
		std::vector<Setting> settings;

		void AddCategory(std::string id, std::string name, std::string description) {
			categories.push_back({ std::move(id), std::move(name), std::move(description) });
		}
		// Settings added after this go in this section (of the last category added)
		void AddSection(std::string name, std::string description = "", eLayout layout = eLayout::ROWS, std::optional<Condition> condition = std::nullopt) {
			sections.push_back({ categories.back().id, std::move(name), std::move(description), layout, std::move(condition) });
		}
		void Add(Setting setting) {
			setting.section = sections.back().name;
			settings.push_back(std::move(setting));
		}
	};

	Catalog Build() {
		Catalog c;

		// ================= Server =================
		c.AddCategory("server", "Server", "Where data lives, how the servers find each other and the players, and logging.");

		c.AddSection("Database", "Only in the .ini files or the environment: the servers need it before they can read anything else.");
		c.Add(Choice(SHARED, "database_type", "Database", "Where game data is stored.", "sqlite", { "sqlite", "mysql" }, true));
		c.Add(When(Format(Text(SHARED, "sqlite_database_path", "SQLite file", "Relative to the server binaries.", "", true), eFormat::PATH), SHARED, "database_type", { "sqlite" }));

		c.AddSection("MySQL", "The MySQL or MariaDB server.", eLayout::ROWS, Condition{ SHARED, "database_type", { "mysql" } });
		c.Add(Format(Text(SHARED, "mysql_host", "Host", "Host name or address.", "", true), eFormat::HOST));
		c.Add(Text(SHARED, "mysql_database", "Database name", "", "", true));
		c.Add(Text(SHARED, "mysql_username", "User", "", "", true));
		c.Add(Secret(SHARED, "mysql_password", "Password", "", true));

		c.AddSection("Network", "Addresses and ports. Changing a port needs a restart of every server.");
		c.Add(Format(Text(SHARED, "external_ip", "Public address", "The address players connect to. localhost for a server only you play on.", "localhost", true), eFormat::HOST));
		c.Add(Format(Text(SHARED, "bind_ip", "Listen address", "The local IPv4 address the servers listen on. Empty for all interfaces; players are still sent the public address.", "", true), eFormat::HOST));
		c.Add(Format(Text(MASTER, "master_ip", "Master address", "The address the other servers use to reach master.", "localhost", true), eFormat::HOST));
		c.Add(Port(MASTER, "master_server_port", "Master port", "", "2000"));
		c.Add(Port(AUTH, "auth_server_port", "Auth port", "The retail client always connects to 1001.", "1001"));
		c.Add(Port(SHARED, "chat_server_port", "Chat port", "Used by the chat and world servers.", "2005"));
		c.Add(Port(MASTER, "world_port_start", "First world port", "World servers take ports counting up from here.", "3000"));
		c.Add(Secret(MASTER, "master_password", "Master password", "Shared by the servers to connect to master. Change it on public servers.", true));
		c.Add(Int(SHARED, "max_clients", "Maximum connections", "How many clients each server accepts at once.", "999", 1, 10000, true));
		c.Add(Unit(Int(SHARED, "maximum_mtu_size", "Packet size (MTU)", "Largest packet sent. Lower it if players get stuck at 55% loading.", "1228", 512, 1492, true), "bytes"));
		c.Add(Unit(Int(SHARED, "maximum_outgoing_bandwidth", "Outgoing bandwidth limit", "0 means no limit. Raise it if enemies lag behind players.", "0", 0, std::nullopt, true), "bits/s"));

		c.AddSection("Startup");
		c.Add(Bool(MASTER, "prestart_servers", "Start auth, chat and char servers", "Master starts the other servers itself.", true, true));
		{
			auto worlds = List(MASTER, "prestart_worlds", "Worlds started with master", "Started when master starts, before anyone asks for them. Empty: character select (0) and %[ZoneTable_1000_DisplayDescription] (1000).", eListOf::ZONE, "0,1000");
			worlds.restart = true;
			c.Add(When(std::move(worlds), MASTER, "prestart_servers", { "1" }));
		}
		c.Add(Bool(MASTER, "enable_dashboard", "Start the web dashboard", "", false, true));
		c.Add(Bool(SHARED, "skip_account_creation", "Skip the first-account prompt", "For non-interactive setups: master doesn't ask for an account when there are none.", false, true));

		c.AddSection("Live updates", "Moving every server and world instance onto a new build without a restart (the dashboard's Live update, /liveupdate or SIGUSR2 to master). Read when one starts.");
		c.Add(Unit(Int(MASTER, "live_update_warn_seconds", "Warning before moving players", "The game's Mythran maintenance warning is shown this long before players are moved. The dashboard can pick another for one update.", "10", 0, 300), "seconds"));
		c.Add(Int(MASTER, "live_update_parallel_worlds", "Worlds at once", "World instances replaced at the same time.", "4", 1, 64));
		c.Add(Unit(Int(MASTER, "live_update_player_wait", "Wait for busy players", "Players who are dead or building are moved once they are done, or after this long.", "30", 0, 600), "seconds"));
		c.Add(Unit(Int(MASTER, "live_update_property_build_wait", "Wait for property builders", "A property is saved for its new instance once nobody builds there, or after this long (they leave build mode then).", "60", 0, 600), "seconds"));
		c.Add(Unit(Int(MASTER, "live_update_char_select_wait", "Wait at character select", "Players picking a character get this long to go in by themselves; then they are moved to the new character select.", "60", 0, 3600), "seconds"));
		c.Add(Unit(Int(MASTER, "live_update_activity_wait", "Wait for activities", "Races, minigames and other activity zones get this long to finish; then their players are moved (the activity is lost).", "1800", 0, 86400), "seconds"));
		c.Add(Unit(Int(MASTER, "live_update_ugc_drain_timeout", "Wait for the UGC server", "The UGC server finishes the models it is making; after this long it is stopped (they are made again).", "300", 0, 3600), "seconds"));
		c.Add(Unit(Int(MASTER, "live_update_service_timeout", "Server restart timeout", "How long auth, chat, the UGC server or the dashboard may take to stop or come back before master starts it again (3 tries).", "30", 10, 600), "seconds"));
		c.Add(Bool(MASTER, "live_update_run_migrations", "Run database migrations", "Run the new build's database migrations first. The running servers must cope with the new schema until they are replaced.", true));

		c.AddSection("Game client");
		c.Add(Format(Text(SHARED, "client_location", "Client folder", "The folder with res/, or the one with client/ and versions/.", "", true), eFormat::PATH));
		c.Add(Int(SHARED, "client_net_version", "Network version", "Clients reporting another version are refused. 171022 for the retail client, 171023 for Darkflame Universe clients.", "171022", 0, std::nullopt, true));
		c.Add(Int(SHARED, "version_major", "Version: major", "Sent to the client at login and used for level gating.", "1", 0, 65535));
		c.Add(Int(SHARED, "version_current", "Version: current", "", "10", 0, 65535));
		c.Add(Int(SHARED, "version_minor", "Version: minor", "", "64", 0, 65535));

		c.AddSection("Logging and crashes");
		c.Add(Bool(SHARED, "log_to_console", "Log to the console", "Also print log lines to the terminal.", true, true));
		c.Add(Bool(SHARED, "log_debug_statements", "Debug logging", "Extra log lines for developers.", false, true));
		c.Add(Format(Text(SHARED, "dump_folder", "Crash dump folder", "Where crash logs go. Empty turns them off.", "", true), eFormat::PATH));
		c.Add(Bool(WORLD, "generate_dump", "Crash dumps from world servers", "Write a dump when a world server crashes (needs the crash dump folder).", false, true));
		c.Add(Bool(WORLD, "save_lxfmls", "Save model files", "Save players' models (LXFML) to disk before they are split, for debugging.", false));
		c.Add(Bool(WORLD, "ghosting_scenes", "Scene ghosting", "Players get the objects of the scenes their game keeps loaded (the scene under them, the scenes connected to it and the global scene), as the client streams scenes, instead of the ones within 100 to 150 units. Needs the zone's terrain scene map; zones without one keep distance ghosting.", true, true));
		c.Add(Bool(WORLD, "save_property_location", "Log back in on properties", "Logging out on a property and back in returns to that property. Off (as live): to the last world before it.", false));
		c.Add(Bool(WORLD, "bbb_consume_bricks", "Brick building uses bricks", "Saving a brick by brick model uses up the bricks in it, as live did. Off: the bricks go back to the backpack.", false));
		c.Add(Bool(SHARED, "dont_generate_dcf", "Don't build the chat filter file", "Skip compiling the chat word list to a file.", false, true));

		c.AddSection("Chat web API", "A small HTTP API on the chat server, on localhost only.");
		c.Add(Unused(Bool(CHAT, "web_server_enabled", "Chat web API", "Not read: the chat server's web API was removed; the dashboard's API covers players, teams and announcements.", false, true)));
		c.Add(Unused(Port(CHAT, "web_server_port", "Chat web API port", "Not read: the chat server's web API was removed.", "2005")));
		c.Add(Unused(Port(CHAT, "web_server_listen_port", "Port (old name)", "Not read.", "2005")));

		c.AddSection("Physics", "How world servers split zones for collision checks.");
		c.Add(Bool(WORLD, "phys_spatial_partitioning", "Spatial partitioning", "Faster collision checks. Leave on unless debugging physics.", true, true));
		c.Add(When(Int(WORLD, "phys_sp_tilesize", "Tile size", "Keep tile size / tile count at 205/12 (1:1 with LU's terrain); 102/24 or 154/18 are faster.", "205", 1, 10000, true), WORLD, "phys_spatial_partitioning", { "1" }));
		c.Add(When(Int(WORLD, "phys_sp_tilecount", "Tile count", "", "12", 1, 1000, true), WORLD, "phys_spatial_partitioning", { "1" }));

		// ================= Gameplay =================
		c.AddCategory("gameplay", "Gameplay", "What players can do in game.");

		c.AddSection("Rules");
		c.Add(Labels(Choice(SHARED, "default_team_loot", "Team loot", "How new teams share loot. Players can still change it in their team.", "1", { "0", "1" }),
			{ "Shared loot (like live)", "Free for all" }));
		c.Add(Bool(WORLD, "disable_chat", "Turn off chat", "Nobody can chat in game.", false, true));
		c.Add(Bool(WORLD, "disable_extra_backpack", "No extra backpack space", "DLU gives 2 extra slots per level up; this turns that off.", false));
		c.Add(Bool(WORLD, "disable_vanity", "No vanity NPCs", "Hide DLU's extra NPCs and plaques.", false, true));
		c.Add(Bool(WORLD, "solo_racing", "Solo racing", "Allow racing alone.", false));
		c.Add(Bool(WORLD, "classic_survival_scoring", "Classic survival scoring", "Rank %[ZoneTable_1101_DisplayDescription] by time, like live, instead of score.", false));
		c.Add(Bool(WORLD, "pets_take_imagination", "Pets use imagination", "Like live.", false));
		c.Add(Bool(WORLD, "allow_nameplate_off", "Players can hide their nameplate", "Staff always can; this lets players (GM 0) turn off the name above their head too.", false));
		c.Add(Bool(WORLD, "allow_players_to_skip_cinematics", "Players can skip cinematics", "Most cutscenes get a skip option.", false));
		c.Add(Bool(WORLD, "auto_reject_empty_properties", "Reject empty properties", "Properties made public without models are rejected automatically.", false));
		c.Add(Bool(WORLD, "property_bff_build", "Best friends build on properties", "Best friends of a property's owner can join the owner's build mode there: place, move and pick up models, build brick by brick and edit behaviors. Only the owner starts build mode. Off (as live): only the owner builds.", false));
		c.Add(Unused(Bool(WORLD, "disable_drops", "Turn off loot drops", "Not read by this version.", false)));

		c.AddSection("Hardcore mode", "Players lose items, coins and U-score when they die, and earn extra U-score from enemies.");
		c.Add(Bool(WORLD, "hardcore_mode", "Hardcore mode", "Its settings appear below when it's on.", false));
		c.Add(When(Bool(WORLD, "hardcore_dropinventory_on_death", "Drop everything on death", "Inventory and coins drop on the ground and can be picked up again.", false), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(Unit(Int(WORLD, "hardcore_lose_uscore_on_death_percent", "U-score lost on death", "", "10", 0, 100), "%"), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(Float(WORLD, "hardcore_coin_keep", "Coins kept on death", "Fraction of coins kept, 0 to 1.", "0", 0, 1), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(Unit(Int(WORLD, "hardcore_uscore_enemies_multiplier", "U-score per enemy", "Enemies give their max health times this. 0 turns it off.", "2", 0, 1000), "× health"), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(List(WORLD, "hardcore_excluded_item_drops", "Items never dropped", "Kept on death.", eListOf::LOT), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(List(WORLD, "hardcore_disabled_worlds", "Worlds without hardcore", "", eListOf::ZONE), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(Float(WORLD, "hardcore_uscore_reduction", "U-score reduction", "Multiplier for enemies in the worlds and of the kinds below (e.g. 0.5).", "1", 0, 100), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(List(WORLD, "hardcore_uscore_reduced_worlds", "Reduced U-score worlds", "Every enemy here gives reduced U-score.", eListOf::ZONE), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(List(WORLD, "hardcore_uscore_reduced_lots", "Reduced U-score enemies", "", eListOf::LOT), WORLD, "hardcore_mode", { "1" }));
		c.Add(When(List(WORLD, "hardcore_uscore_excluded_enemies", "Enemies without U-score", "", eListOf::LOT), WORLD, "hardcore_mode", { "1" }));

		c.AddSection("Property rent", "Owners pay rent for their properties, like live: each property world's PropertyTemplate price and period, or what the Property Rent page sets. %[ZoneTable_1150_DisplayDescription] is free.");
		c.Add(Bool(WORLD, "property_rent_enabled", "Charge rent", "Rent is taken from the owner's coins when they log in. Unpaid rent makes the property private until it is paid.", false));
		c.Add(When(Unit(Int(WORLD, "property_rent_grace_days", "Grace period", "How long rent can be unpaid before the property is made private.", "3", 0, 365), "days"), WORLD, "property_rent_enabled", { "1" }));

		c.AddSection("Property reputation", "Visitors earn properties reputation, which orders the property lists and the news screen's Today's Top Properties. "
			"Only time other people spend moving around a property counts, with limits against farming it (see the dashboard documentation).");
		c.Add(Bool(WORLD, "property_reputation_enabled", "Properties earn reputation", "", true));
		c.Add(When(Unit(Int(WORLD, "property_reputation_min_visit", "Minimum visit", "A visit earns nothing before this (live's property reputation delay).", "120", 0, 3600), "seconds"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Unit(Float(WORLD, "property_reputation_multiplier", "Points per minute", "Times the property's reputationPerMinute (1). Raise it for small servers.", "1", 0, 1000), "×"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Unit(Int(WORLD, "property_reputation_max_minutes", "Minutes per visit", "At most this many minutes of one visit count.", "30", 1, 1440), "minutes"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Unit(Int(WORLD, "property_reputation_visitor_daily_cap", "Per visitor per day", "Most one account can give one property in a day.", "30", 0, 100000), "points"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Unit(Int(WORLD, "property_reputation_daily_cap", "Per property per day", "Most a property can get in a day from everyone.", "300", 0, 10000000), "points"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Float(WORLD, "property_reputation_repeat_falloff", "Repeat visitor falloff", "A visitor who gave the property reputation on d recent days earns 1 / (1 + this × d) as much.", "0.5", 0, 100), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Unit(Int(WORLD, "property_reputation_repeat_days", "Recent days", "How far back repeat visits are counted.", "30", 1, 365), "days"), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Bool(WORLD, "property_reputation_require_activity", "Only active visitors", "A minute only counts if the visitor moved; idle characters earn nothing.", true), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Bool(WORLD, "property_reputation_ignore_staff", "Ignore staff", "Accounts with a GM level don't give reputation.", true), WORLD, "property_reputation_enabled", { "1" }));
		c.Add(When(Bool(WORLD, "property_reputation_ignore_linked", "Ignore linked accounts", "Accounts sharing a play key, email or login address with the owner's don't give reputation.", true), WORLD, "property_reputation_enabled", { "1" }));

		c.AddSection("Events", "Event flags sent at login; they switch on event content in levels (e.g. Talk_Like_A_Pirate).", eLayout::GRID);
		for (int i = 1; i <= 8; i++) c.Add(Text(SHARED, "event_" + std::to_string(i), "Event " + std::to_string(i), "", ""));

		c.AddSection("Help menu", "The five entries of the in-game Help menu. The text can use the client's simple HTML (<br/>, <a>, <font>).", eLayout::PAIRS);
		for (int i = 0; i < 5; i++) {
			c.Add(Text(WORLD, "help_" + std::to_string(i) + "_summary", "Entry " + std::to_string(i + 1) + " title", "", ""));
			c.Add(Multiline(Text(WORLD, "help_" + std::to_string(i) + "_description", "Entry " + std::to_string(i + 1) + " text", "", "")));
		}

		c.AddSection("Client checks");
		c.Add(Bool(WORLD, "check_fdb", "Check the client database", "Compare each client's CDClient (res/CDServer.fdb) with the server's.", false));
		c.Add(When(Text(WORLD, "cdclient_mismatch_title", "Mismatch title", "Shown to players whose client database differs.", ""), WORLD, "check_fdb", { "1" }));
		c.Add(When(Multiline(Text(WORLD, "cdclient_mismatch_message", "Mismatch message", "", "")), WORLD, "check_fdb", { "1" }));

		c.AddSection("About");
		c.Add(Format(Text(WORLD, "source", "Source code link", "Where your server's code is. If you changed it, link your version (AGPLv3).", "https://github.com/DarkflameUniverse/DarkflameServer"), eFormat::URL));

		// ================= Players and access =================
		c.AddCategory("players", "Players and access", "Who can log in, friends, and automatic moderation.");

		c.AddSection("Logging in");
		c.Add(Bool(AUTH, "dont_use_keys", "Don't require play keys", "Everyone with an account can log in, with or without a play key.", false));
		c.Add(Bool(AUTH, "closed_to_non_devs", "Staff only", "Only accounts with a GM level can log in.", false));
		c.Add(List(AUTH, "rewardcodes", "Reward codes", "Given to every account at login. 4 opens %[ZoneTable_1700_DisplayDescription]; 30 stops bricks being used up in build mode. Codes with an item mail it once.", eListOf::REWARD_CODE, "4,30"));
		c.Add(Bool(AUTH, "log_login_addresses", "Remember login addresses", "Keep the network address each account logs in to the game from, so staff see accounts that share one. "
			"Addresses are personal data: they're only shown as links between accounts, and deleted after log_login_address_days.", true));
		c.Add(Bool(AUTH, "log_client_sysinfo", "Remember client system info", "Keep the system description the client sends when it logs in (Windows version, video card, "
			"processor count, memory), as the client reported it, for staff with client_sysinfo. The client's old Windows calls often report "
			"compatibility values, not the real hardware. Deleted after log_client_sysinfo_days.", true));

		c.AddSection("Moderation");
		c.Add(Bool(SHARED, "mute_restrict_mail", "Muted players can't send mail", "", false));
		c.Add(Bool(SHARED, "mute_restrict_trade", "Muted players can't trade", "", false));
		c.Add(Bool(SHARED, "mute_auto_reject_names", "Reject names from muted players", "Character and pet names asked for while muted are rejected automatically.", false));
		c.Add(Bool(WORLD, "disable_anti_speedhack", "Turn off speedhack detection", "Try this if players get kicked for lag.", false, true));
		c.Add(Unused(Bool(WORLD, "log_ip_addresses_for_anti_cheat", "Log IP addresses", "Not read by this version.", true)));

		c.AddSection("Chat log", "What players say is kept for moderators (the Chat Log page) and for chat bridges.");
		c.Add(Bool(SHARED, "log_chat", "Keep a chat log", "Zone chat, and what the chat filter stopped.", true));
		c.Add(When(Bool(SHARED, "log_private_chat", "Include whispers and team chat", "Only staff with the Read private chat permission see them.", true), SHARED, "log_chat", { "1" }));
		c.Add(Bool(WORLD, "chat_bridge_filter", "Filter messages sent into the game", "Messages from the dashboard or a chat bridge go through the same chat filter as players'.", true));

		c.AddSection("Friends");
		c.Add(Int(CHAT, "max_number_of_friends", "Friends", "50 is live accurate; more needs a modded client in places.", "50", 0, 1000, true));
		c.Add(Int(CHAT, "max_number_of_best_friends", "Best friends", "5 is live accurate.", "5", 0, 1000, true));
		c.Add(Int(CHAT, "max_ignores", "Ignore list size", "", "32", 0, 1000));

		c.AddSection("Guilds", "docs/Guilds.md. Players also need the client's FeatureGating row \"guilds\".");
		c.Add(Int(CHAT, "guild_max_members", "Guild members", "The client has no limit of its own.", "100", 1, 1000, true));
		c.Add(Int(CHAT, "guild_invite_timeout", "Seconds to answer a guild invite", "", "600", 10, 86400, true));

		c.AddSection("Dashboard sign-in");
		c.Add(Int(DASHBOARD, "min_dashboard_gm_level", "Lowest GM level allowed in", "0 lets every player see their own account.", "0", 0, 9));
		c.Add(Int(DASHBOARD, "require_2fa_gm_level", "Two-factor login required from GM level", "Staff at or above this level must use an authenticator app. 0 turns it off.", "0", 0, 9));
		c.Add(Text(DASHBOARD, "totp_issuer", "Name in authenticator apps", "", "DarkflameServer"));
		c.Add(Bool(DASHBOARD, "password_reset_recovery_codes", "Password reset with recovery codes",
			"Players with two-factor login can choose a new password on /forgot_password with a code from their authenticator app and one of their recovery codes, no email needed.", true));

		c.AddSection("Registration", "Creating accounts on the dashboard at /register.");
		c.Add(Bool(DASHBOARD, "allow_registration", "Anyone can register", "", false));
		c.Add(When(Bool(DASHBOARD, "registration_requires_play_key", "Needs a play key", "", true), DASHBOARD, "allow_registration", { "1" }));
		c.Add(When(Bool(DASHBOARD, "registration_requires_email", "Needs an email address", "Only when email is set up.", false), DASHBOARD, "allow_registration", { "1" }));

		// ================= Email =================
		c.AddCategory("email", "Email", "For password resets, confirmations and report emails. Test it with Send Test under Email Settings on your account page.");

		c.AddSection("Mail server");
		c.Add(Format(Text(DASHBOARD, "smtp_host", "SMTP server", "Filled in for Microsoft or Google OAuth2.", "", true), eFormat::HOST));
		c.Add(Int(DASHBOARD, "smtp_port", "Port", "587 with starttls, 465 with tls.", "587", PORT_MIN, PORT_MAX, true));
		c.Add(Choice(DASHBOARD, "smtp_security", "Encryption", "none only for a relay on the same machine.", "starttls", { "starttls", "tls", "none" }, true));
		c.Add(Choice(DASHBOARD, "smtp_auth", "Sign-in", "oauth2 is needed for Microsoft 365 and recommended for Gmail.", "password", { "password", "oauth2" }, true));
		c.Add(When(Text(DASHBOARD, "smtp_username", "User", "", "", true), DASHBOARD, "smtp_auth", { "password" }));
		c.Add(When(Secret(DASHBOARD, "smtp_password", "Password", "", true), DASHBOARD, "smtp_auth", { "password" }));
		c.Add(Format(Text(DASHBOARD, "smtp_from_address", "From address", "", "", true), eFormat::EMAIL));
		c.Add(Text(DASHBOARD, "smtp_from_name", "From name", "", "DarkflameServer", true));
		c.Add(Format(Text(DASHBOARD, "smtp_ca_file", "Trusted certificates", "A PEM bundle for a private CA. Empty uses the system's.", "", true), eFormat::PATH));
		c.Add(Bool(DASHBOARD, "smtp_verify_certificate", "Check the server's certificate", "Turn off only for local testing.", true, true));
		c.Add(Unit(Int(DASHBOARD, "smtp_timeout", "Timeout", "", "20", 1, 300, true), "seconds"));

		const Condition custom{ DASHBOARD, "smtp_oauth2_provider", { "custom" } };
		c.AddSection("OAuth2 sign-in", "After filling these in, use Connect mail account under Email Settings on your account page.", eLayout::ROWS, Condition{ DASHBOARD, "smtp_auth", { "oauth2" } });
		c.Add(Choice(DASHBOARD, "smtp_oauth2_provider", "Provider", "microsoft and google fill in the rest.", "custom", { "microsoft", "google", "custom" }, true));
		c.Add(Text(DASHBOARD, "smtp_oauth2_client_id", "Client ID", "", "", true));
		c.Add(Secret(DASHBOARD, "smtp_oauth2_client_secret", "Client secret", "", true));
		c.Add(Choice(DASHBOARD, "smtp_oauth2_grant", "Grant", "client_credentials is Microsoft 365 app-only sending.", "authorization_code", { "authorization_code", "client_credentials" }, true));
		c.Add(When(Text(DASHBOARD, "smtp_oauth2_tenant", "Microsoft tenant", "Tenant ID or domain; empty uses common.", "", true), DASHBOARD, "smtp_oauth2_provider", { "microsoft" }));
		c.Add(When(Format(Text(DASHBOARD, "smtp_oauth2_authorize_url", "Authorize URL", "", "", true), eFormat::URL), custom.file, custom.key, custom.values));
		c.Add(When(Format(Text(DASHBOARD, "smtp_oauth2_token_url", "Token URL", "", "", true), eFormat::URL), custom.file, custom.key, custom.values));
		c.Add(When(Text(DASHBOARD, "smtp_oauth2_scope", "Scope", "", "", true), custom.file, custom.key, custom.values));
		c.Add(Secret(DASHBOARD, "smtp_oauth2_refresh_token", "Refresh token", "Set by Connect mail account; only set it by hand for a token from elsewhere.", true));

		// ================= Dashboard =================
		c.AddCategory("dashboard", "Dashboard", "The web dashboard itself.");

		c.AddSection("Web server");
		c.Add(Port(DASHBOARD, "port", "Web port", "Where the dashboard listens for browsers.", "2006"));
		c.Add(Format(Text(DASHBOARD, "listen_ip", "Listen address", "127.0.0.1 for this machine only (put a reverse proxy in front); 0.0.0.0 for everyone.", "127.0.0.1", true), eFormat::HOST));
		c.Add(Port(DASHBOARD, "net_port", "Server link port", "UDP port for the connection to master (the next one is used too). Keep clear of other servers' ports.", "2010"));
		c.Add(Format(Text(DASHBOARD, "dashboard_url", "Public address", "For links in emails and alerts, e.g. https://dashboard.example.com.", ""), eFormat::URL));
		c.Add(Bool(DASHBOARD, "secure_cookies", "HTTPS only cookies", "Turn on when the dashboard is served over HTTPS.", false));
		c.Add(Bool(DASHBOARD, "behind_proxy", "Behind a reverse proxy", "Use the proxy's X-Forwarded-For for rate limits. Only when the dashboard can't be reached directly.", false));
		c.Add(Unit(Int(DASHBOARD, "broadcast_interval", "Live update interval", "How often server status is pushed to open pages.", "2000", 250, 60000, true), "ms"));
		c.Add(Unit(Int(DASHBOARD, "scenery_workers", "3D model conversion threads",
			"Threads converting the client's models for the 3D views, so the dashboard keeps answering meanwhile. One of them only takes flairs and small models. 0 picks half the CPU cores (2 to 4).",
			"0", 0, 16, true), "threads"));

		c.AddSection("Keys", "Only in the .ini file or the environment.");
		c.Add(Secret(DASHBOARD, "jwt_secret", "Session signing secret", "At least 32 characters; empty makes one. Changing it signs everyone out.", true));
		c.Add(Secret(DASHBOARD, "totp_key", "Two-factor encryption key", "64 hex characters; empty uses dashboard_totp_key. Back it up.", true));

		c.AddSection("Tools");
		c.Add(Text(DASHBOARD, "challenge_milestones", "Challenge milestones", "Percentages of a community challenge announced in game as it gets there, e.g. 25,50,75 (completing it is always announced).", "25,50,75,100"));
		c.Add(Bool(DASHBOARD, "enable_char_xml_upload", "Character XML editing", "Let staff with permission replace a character's XML.", false));

		c.AddSection("Public pages", "Pages anyone can open without signing in. They never show account names, addresses or anything else private.");
		c.Add(Bool(DASHBOARD, "public_status", "Public server status", "The page at /status, its JSON at /api/public/status (for server lists) and a widget for other sites.", false));
		c.Add(When(Text(DASHBOARD, "public_server_name", "Server name", "Shown on the status page and widget.", "DarkflameServer"), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_players", "Players per world", "How many players are in each world.", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_names", "Names of players online", "Character names (never staff, never account names) by world.", false), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_uptime", "Uptime", "How long the server has been up, and how much of the last day and week.", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_health", "Server health", "Whether login and chat are up, and how many worlds are running.", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Unit(Int(DASHBOARD, "public_status_leaderboard_top", "Leaderboard places", "The top places of every leaderboard. 0 leaves leaderboards out.", "3", 0, 10), "places"), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_challenges", "Community challenges", "Public challenges that are running or finished in the last week, with their progress.", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_events", "Live events", "Live events running now (their title, where, and until when).", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Bool(DASHBOARD, "public_status_widget", "Widget for other sites", "A small page at /status/widget other sites may show in an iframe.", true), DASHBOARD, "public_status", { "1" }));
		c.Add(When(Unit(Int(DASHBOARD, "public_status_cache_seconds", "Refresh every", "The status is worked out at most this often, however many people ask.", "60", 10, 3600), "seconds"), DASHBOARD, "public_status", { "1" }));
		c.Add(Bool(DASHBOARD, "showcase_public", "Property showcase for everyone", "Let people who aren't signed in browse approved public properties at /showcase. Signed-in players need the showcase_view permission.", false));

		c.AddSection("Metrics", "Prometheus metrics at /metrics: players, worlds, memory, chat, today's economy, moderation queues and scheduled tasks. Never names or addresses.");
		c.Add(Unit(Int(DASHBOARD, "api_key_rate_limit", "API key rate limit", "Requests a minute an API key may make unless the key sets its own limit (up to 6000).", "120", 1, 6000), "/min"));
		c.Add(Bool(DASHBOARD, "metrics_enabled", "Metrics endpoint", "Off: /metrics answers 404. On: scrapers send an API token of an account with metrics_view, or the token below.", false));
		c.Add(When(Secret(DASHBOARD, "metrics_token", "Scraper token", "A shared secret scrapers may send as Authorization: Bearer instead of an API token. At least 16 characters; empty: API tokens only."), DASHBOARD, "metrics_enabled", { "1" }));
		c.Add(When(Text(DASHBOARD, "metrics_allowed_ips", "Allowed addresses", "Comma separated addresses or IPv4 ranges (10.0.0.0/8) that may fetch metrics. Empty: any."), DASHBOARD, "metrics_enabled", { "1" }));
		c.Add(When(Unit(Int(DASHBOARD, "metrics_cache_seconds", "Refresh every", "Metrics are worked out at most this often, however often they are fetched.", "10", 1, 300), "seconds"), DASHBOARD, "metrics_enabled", { "1" }));

		c.AddSection("Strikes", "Given by staff when rejecting a name, pet name or property, removing a score or acting on a player report, if they choose to. The steps below happen on their own the first time an account reaches that many active strikes.");
		c.Add(Days("strike_expiry_days", "Strikes count for", "Older strikes stay on the account's record but no longer count. 0: they always count.", "0"));
		c.Add(Unit(Int(DASHBOARD, "strike_warn_at", "Warn at", "When an account reaches this many active strikes it gets a warning, shown to the player if they're online. 0: never.", "0", 0, 1000), "strikes"));
		c.Add(Unit(Int(DASHBOARD, "strike_mute_at", "Mute at", "...is muted for the days below. 0: never.", "0", 0, 1000), "strikes"));
		c.Add(Days("strike_mute_days", "Mute for", "", "3", 1));
		c.Add(Unit(Int(DASHBOARD, "strike_ban_at", "Ban at", "...is banned for the days below. 0: never.", "0", 0, 1000), "strikes"));
		c.Add(Days("strike_ban_days", "Ban for", "0: permanently.", "7"));

		c.AddSection("AI moderator helper", "A Suggest button on player reports, chat, pending names and economy flags that asks Claude (Anthropic's API) to draft a suggestion for staff (ai_suggest permission). Only staff see it; nothing is applied or shown to players unless staff do it themselves. Each request sends the item, the player's recent chat around it, their strikes and moderation history and the rules below; never emails, addresses or passwords.");
		c.Add(Bool(DASHBOARD, "ai_helper_enabled", "AI moderator helper", "Off: the Suggest buttons say the helper is off and nothing is sent.", false));
		c.Add(Secret(DASHBOARD, "claude_api_key", "Claude API key", "From console.anthropic.com. Never shown back or logged."));
		c.Add(Format(Text(DASHBOARD, "claude_api_base", "API address", "Only change it for a proxy or a local test server (http:// only for this machine).", "https://api.anthropic.com"), eFormat::URL));
		{
			auto model = Text(DASHBOARD, "claude_model", "Model", "Any model ID works. claude-sonnet-5 is a good balance; claude-haiku-4-5-20251001 is cheaper, claude-opus-5-5 more careful.", "claude-sonnet-5");
			model.choices = { "claude-sonnet-5", "claude-opus-5-5", "claude-haiku-4-5-20251001" };
			c.Add(model);
		}
		c.Add(Bool(DASHBOARD, "claude_structured_output", "Hold answers to a schema", "Ask the API for JSON of the expected shape (structured outputs). Turn off only if a model refuses it; answers are checked either way.", true));
		c.Add(Multiline(Text(DASHBOARD, "ai_helper_rules", "Server rules", "Your code of conduct, as players know it. Sent with every request so suggestions follow your rules.")));
		c.Add(Unit(Int(DASHBOARD, "ai_helper_timeout", "Time out after", "Per try; failed tries (busy API, network) are retried up to three times.", "60", 5, 600), "seconds"));
		c.Add(Unit(Int(DASHBOARD, "ai_helper_max_tokens", "Longest answer", "max_tokens per request: caps what one suggestion can cost.", "2000", 256, 16000), "tokens"));
		c.Add(Unit(Int(DASHBOARD, "ai_helper_per_minute", "Requests per minute", "For the whole dashboard.", "5", 1, 60), "requests"));
		c.Add(Unit(Int(DASHBOARD, "ai_helper_per_day", "Requests per day", "For the whole dashboard, per UTC day. Asking about the same thing again reuses the stored suggestion and costs nothing.", "200", 1, 100000), "requests"));
		c.Add(Unit(Int(DASHBOARD, "ai_helper_chat_minutes", "Chat around the item", "How far before and after a report or message the player's chat is included.", "10", 1, 1440), "minutes"));

		c.AddSection("Economy checks", "Flags for the Economy page, checked every night.");
		c.Add(Unit(Int(DASHBOARD, "anomaly_coin_multiplier", "Coin income: times the median", "Flag characters earning more than this many times the median player in a day...", "20", 1, 100000), "×"));
		c.Add(Unit(Int(DASHBOARD, "anomaly_coin_minimum", "Coin income: at least", "...and more than this many.", "100000", 0, std::nullopt), "coins"));
		c.Add(Unit(Int(DASHBOARD, "anomaly_item_multiplier", "Item spike: times usual", "Flag items created more than this many times their daily average...", "10", 1, 100000), "×"));
		c.Add(Unit(Int(DASHBOARD, "anomaly_item_minimum", "Item spike: at least", "...and more than this many.", "200", 0, std::nullopt), "items"));
		c.Add(Bool(DASHBOARD, "economy_duplicate_scan", "Nightly duplicate scan", "Reads every character's inventory.", true));

		c.AddSection("Contraband", "Items on the Contraband page are flagged, or removed, when a character loads or receives one. The list itself is edited on that page.");
		c.Add(Bool(WORLD, "contraband_ignore_staff", "Don't check staff", "Accounts with a GM level keep listed items and aren't flagged.", true));
		c.Add(Bool(WORLD, "contraband_notify_players", "Tell players", "A mail (at login) or a chat message (when received) says which item was removed and why.", true));

		c.AddSection("Backups");
		c.Add(Format(Text(DASHBOARD, "backup_folder", "Backup folder", "Relative to the server binaries unless absolute.", "backups"), eFormat::PATH));
		c.Add(Unit(Int(DASHBOARD, "backup_keep", "Backups to keep", "Older backups are deleted after each new one. 0 keeps them all.", "7", 0, 1000), "backups"));
		c.Add(When(Format(Text(DASHBOARD, "backup_mysqldump", "mysqldump program", "The mysqldump (or mariadb-dump) to run.", "mysqldump"), eFormat::PATH), SHARED, "database_type", { "mysql" }));

		// ================= Data retention =================
		c.AddCategory("retention", "Data retention", "How long history is kept. The Log pruning task deletes older rows every night.");

		c.AddSection("Logs", "0 keeps everything.");
		c.Add(Days("log_activity_days", "Activity log", "", "365"));
		c.Add(Days("log_command_days", "Command log", "", "365"));
		c.Add(Days("log_audit_days", "Audit log", "", "730"));
		c.Add(Days("log_cheat_detection_days", "Cheat detections", "", "365"));
		c.Add(Days("log_task_days", "Scheduled task runs", "", "90"));
		c.Add(Days("log_chat_days", "Chat log", "Every message players sent (zone, whispers, team and guild chat). Chat flags keep a copy of what they flagged.", "30"));
		c.Add(Days("log_login_address_days", "Login addresses", "Addresses an account hasn't logged in from for this long are forgotten.", "90"));
		c.Add(Days("log_client_sysinfo_days", "Client system info", "System descriptions clients sent at login that haven't been seen for this long are forgotten.", "90"));
		c.Add(Days("health_days", "Server health history", "Minute-by-minute player counts and uptime.", "30"));
		c.Add(Days("traffic_days", "Server traffic history", "Minute-by-minute packets, bytes and HTTP requests of every server (Diagnostics). The last hour at one second is only kept in memory.", "30"));

		c.AddSection("Log bundles", "Server log files downloaded together from the System Log page (Download logs).");
		c.Add(Unit(Int(DASHBOARD, "log_bundle_max_mb", "At most", "How much log text (before compression) one download may hold. Bundles are built in the system's temporary folder and deleted once sent.", "512", 1, 4000), "MB"));

		c.AddSection("Player movement", "Where players went, for replays on the 3D world view (staff with players_history only).");
		c.Add(Bool(DASHBOARD, "position_history", "Record player movement", "Keeps a player's position every few seconds while they move, and every 30 seconds while they stand still.", true));
		c.Add(Unit(Int(DASHBOARD, "position_history_seconds", "Record every", "While a player moves. Less often keeps the table smaller; replays then move in straighter lines.", "5", 1, 600), "seconds"));
		c.Add(Days("position_history_days", "Keep movement for", "About 17,000 rows per player online around the clock per day at 5 seconds.", "3", 1));

		c.AddSection("Message inspector", "Saved game message captures (Message Inspector). The Message capture pruning task deletes older ones every night; running captures are never deleted.");
		c.Add(Days("inspector_session_days", "Keep captures for", "0: no age limit.", "30"));
		c.Add(Unit(Int(DASHBOARD, "inspector_max_mb", "At most", "When all saved captures together take more, the oldest are deleted. A busy 15 minute capture can take 50 MB. 0: no size limit.", "1024", 0, 1000000), "MB"));

		c.AddSection("Economy history");
		c.Add(Days("economy_detail_days", "Daily detail", "Older daily rows are merged into months.", "180", 31));
		c.Add(Days("economy_map_days", "Map detail", "", "90", 31));
		c.Add(Days("economy_transfer_days", "Trades and mail", "", "730", 1));

		c.AddSection("Character history");
		c.Add(Days("snapshot_days", "Snapshots", "Older snapshots are deleted, but each character keeps its newest few (below).", "90", 1));
		c.Add(Unit(Int(DASHBOARD, "snapshot_keep", "Always keep per character", "", "10", 1, 1000), "snapshots"));
		// The UGC server's settings, also shown beside what they change on the UGC page (/ugc reads these sections)
		c.AddCategory("ugc", "UGC server", "Makes the meshes and icons of what players build and serves them to the game client. See docs/UgcServer.md; the UGC page shows these beside what they change.");
		c.AddSection("UGC serving", "Where the game client downloads from and where the dashboard reaches the UGC server.");
		c.Add(Bool(MASTER, "enable_ugc_server", "Start the UGC server", "Makes and serves the meshes and icons of what players build (docs/UgcServer.md).", false, true));
		c.Add(Port(UGC, "port", "Download port", "The game client's UGCSERVERPORT.", "2008"));
		c.Add(Format(Text(UGC, "listen_ip", "Listen address", "The game client has to reach it.", "0.0.0.0", true), eFormat::HOST));
		c.Add(Port(UGC, "net_port", "Master connection port", "UDP; the next port is used too.", "2012"));
		c.Add(Text(UGC, "client_path", "Download path", "The game client's UGCSERVERDIR.", "/ugc", true));
		c.Add(Bool(SHARED, "ugc_manifest", "Answer clients without 3D services", "Worlds tell game clients with UGCUSE3DSERVICES=7:0 (the default) the checksums of the files the UGC server made, so they download icons of cars, rockets and models from UGCSERVERIP, UGCSERVERPORT and UGCSERVERDIR in their boot.cfg (default: the patch server's). A client that can't connect there is logged out, so only turn on when every player's boot.cfg points at the UGC server.", false));
		c.Add(When(Bool(SHARED, "ugc_manifest_models", "Placed models from the UGC server", "With the answer to clients without 3D services on: placed player models the UGC server has made are shown with its mesh. Each client still gets every model's LXFML and builds it first (for its collision; the UGC server makes no physics), then is switched to the served mesh a few seconds after it has loaded. Models not made yet stay as the client built them.", false), SHARED, "ugc_manifest", { "1" }));
		c.Add(Format(Text(DASHBOARD, "ugc_internal_url", "UGC server address (internal)", "Where the dashboard itself reaches the UGC server for its status and files. Empty: http://127.0.0.1:2008.", ""), eFormat::URL));
		c.Add(Format(Text(DASHBOARD, "ugc_public_url", "UGC server address (public)", "Only for the \"open on the UGC server\" links, e.g. https://ugc.example.com. The dashboard's own pages don't need the browser to reach it.", ""), eFormat::URL));
		c.AddSection("UGC processing", "When models are made and how much of the machine the workers may use.", eLayout::ROWS, Condition{ MASTER, "enable_ugc_server", { "1" } });
		c.Add(Unit(Int(SHARED, "ugc_debounce_seconds", "Wait after a save", "A saved model is made this long after the owner's last save (their other waiting models wait too), or as soon as a game client asks for it or the owner leaves. 0: right away.", "120", 0, 86400), "seconds"));
		c.Add(Int(UGC, "worker_threads", "Worker threads", "0: half the CPU cores.", "0", 0, 64, true));
		c.Add(Unit(Float(UGC, "max_cpu_percent", "Most CPU", "The workers together average at most this share of all CPU cores; they pause between bits of work to stay under it. 0: no limit.", "0", 0, 100), "%"));
		c.Add(Int(UGC, "worker_nice", "Worker priority", "Linux nice value of the workers: 0 normal, 19 only when nothing else wants the CPU.", "0", 0, 19));
		c.Add(Unit(Int(UGC, "max_memory_mb", "Most memory for jobs", "Jobs whose estimated memory would go past this together wait for running ones; a bigger one runs alone. 0: no limit.", "0", 0, 1000000), "MB"));
		c.Add(Unit(Int(UGC, "max_model_bricks", "Biggest model", "Models with more bricks fail with that reason. 0: no limit.", "0", 0, 1000000), "bricks"));
		c.Add(Text(UGC, "pause_hours", "Pause hours", "Local hours in which no new jobs start, e.g. 18-23 or 22-6. Empty: never.", ""));
		c.Add(Unit(Int(UGC, "poll_interval_ms", "Look for new models every", "", "2000", 100, 600000, true), "ms"));
		c.Add(Int(UGC, "poll_batch", "Models taken at once", "", "32", 1, 1000, true));
		c.Add(Int(UGC, "max_attempts", "Attempts before giving up", "", "3", 1, 100, true));
		c.AddSection("UGC models", "How a model's mesh is made from its bricks (LU Toolbox's steps).", eLayout::ROWS, Condition{ MASTER, "enable_ugc_server", { "1" } });
		c.Add(Labels(Choice(UGC, "color_palette", "Colors", "LU Toolbox's LU palette (unknown colors black), or the client's Materials.xml.", "lu_toolbox", { "lu_toolbox", "brickdb" }), { "LU Toolbox", "Brick database" }));
		c.Add(Unit(Float(UGC, "color_variation", "Color variation", "Each brick's brightness is shifted by up to this much (the same every time the model is made). 0: none.", "5", 0, 100), "%"));
		c.Add(Unit(Float(UGC, "transparent_opacity", "Transparent opacity", "", "58.82", 0, 100), "%"));
		c.Add(Unit(Float(UGC, "color_brightness", "Model color brightness", "The models' vertex colors (not the icons'). 100: as the palette has them; lower is darker. Models already made keep theirs until they are made again.", "100", 0, 200), "%"));
		c.Add(Text(UGC, "transparent_colors", "Always transparent colors", "LEGO color ids drawn transparent whatever Materials.xml says, comma separated: by default 129, the transparent glitter violet, which Materials.xml has opaque (none: no colors). Their opacity is Transparent opacity.", "129"));
		c.Add(Text(UGC, "lods", "Levels of detail", "brickprimitives levels made, 0 (most detailed) to 2, e.g. 0,2.", "0,2"));
		c.Add(Float(UGC, "lod_distance_0", "LOD 0 distance", "", "0", 0, 100000));
		c.Add(Float(UGC, "lod_distance_1", "LOD 1 distance", "", "50", 0, 100000));
		c.Add(Float(UGC, "lod_distance_2", "LOD 2 distance", "", "100", 0, 100000));
		c.Add(Float(UGC, "lod_distance_3", "LOD 3 distance", "", "280", 0, 100000));
		c.Add(Float(UGC, "lod_cull", "Drawn up to", "", "10000", 0, 1000000));
		c.Add(Text(UGC, "shader_opaque", "Opaque shader", "S<shader>_Opaque_Model.", "01"));
		c.Add(Bool(UGC, "combine_transparent", "One shape for all transparent bricks", "Off: each transparent brick is its own shape, so the client can sort them.", false));
		// Metal and glow groups (UgcJobs::Shaders): off keeps the files exactly as before, as live made them
		const std::string notLive = " Not how live looked: live's models were all LEGO plastic (S01). Models already made keep their look until they are made again (Make everything again, or Reprocess).";
		c.Add(Int(UGC, "shader_metal", "Metal shader", "mapShaders id for metal colors (Materials.xml shinySteel and LU Toolbox's metallic ones), in a group S<id>_Metal_Model: 88 is Polished Metal. 0: off, they stay LEGO plastic." + notLive, "88", 0, 9999));
		c.Add(Int(UGC, "shader_brushed", "Brushed steel shader", "mapShaders id for brushed steel colors (Materials.xml brushedSteel and matteSteel; the client's has none) in S<id>_Brushed_Model: 89 is Brushed Steel. 0: off." + notLive, "89", 0, 9999));
		c.Add(Int(UGC, "shader_glow", "Glow shader", "mapShaders id for opaque glowing colors (LU Toolbox's glow colors) in S<id>_Glow_Model, with their plain color and an emissive material: 46 is LEGO-Emissive. Transparent glow stays with the transparent bricks. 0: off." + notLive, "46", 0, 9999));
		c.Add(Float(UGC, "glow_emissive", "Glow emissive strength", "With the glow shader on: the glow shapes' material emissive, how far the emissive shader goes from lit to the plain color (1: fully).", "1", 0, 10));
		c.Add(Text(UGC, "metal_material_types", "Metal material types", "Materials.xml MaterialTypes drawn as metal, comma separated (none: only LU Toolbox's metallic colors).", "shinySteel"));
		c.Add(Text(UGC, "brushed_material_types", "Brushed steel material types", "Materials.xml MaterialTypes drawn as brushed steel, comma separated (none: no such colors).", "brushedSteel,matteSteel"));
		c.Add(Text(UGC, "brushed_colors", "Brushed steel colors", "LEGO color ids drawn as brushed steel whatever their Materials.xml type, comma separated: by default the drum lacquered 298,300,1002,1004 (none: no colors).", "298,300,1002,1004"));
		c.Add(Int(UGC, "shader_glitter", "Glitter shader", "mapShaders id for glitter colors, in S<id>_Glitter_Model and (transparent ones) S<id>_GlitterAlpha_Model: 21 is LEGO-AnimUV, which lays a white fleck texture stored in the model over the color (still: the client never updates a placed model; see the sparkle shader). 0: off, they stay plastic." + notLive, "21", 0, 9999));
		c.Add(Text(UGC, "glitter_material_types", "Glitter material types", "Materials.xml MaterialTypes drawn as glitter, comma separated (none: only the glitter colors below).", "glitter"));
		c.Add(Text(UGC, "glitter_colors", "Glitter colors", "LEGO color ids drawn as glitter whatever their Materials.xml type, comma separated: by default 114,117, which LEGO's color data calls glitter and the client's Materials.xml plain plastic (none: no colors).", "114,117"));
		c.Add(Float(UGC, "glitter_size", "Glitter tile size", "The fleck texture's tile in model units (a stud is 0.8): with the flecks in a tile, how far apart they are (each brick places the tile its own way).", "1.6", 0.1f, 100));
		c.Add(Int(UGC, "glitter_density", "Glitter flecks", "Flecks in one tile of the glitter texture.", "80", 0, 5000));
		c.Add(Float(UGC, "glitter_fleck_size", "Glitter fleck size", "A fleck's diameter in model units (they are cm: 0.05 is half a millimetre, as LEGO's glitter).", "0.05", 0.005f, 1));
		c.Add(Float(UGC, "glitter_fleck_opacity", "Glitter fleck brightness", "Percent: how white the brightest flecks are over the brick's color (each catches the light differently: most are dimmer).", "80", 0, 100));
		c.Add(Int(UGC, "shader_glitter_sparkle", "Glitter sparkle shader", "mapShaders id for the glitter bricks' sparkles, in S<id>_GlitterSparkle_Model over both glitter groups (only with the glitter shader on): 79 is Distortion Directional (Ocean), whose texture layers the client moves every frame on its own, so a sparkle flashes where two layers' sparkles meet. Nothing else moves on a placed model (the client never updates it). 0: no sparkles.", "79", 0, 9999));
		c.Add(Float(UGC, "glitter_sparkle_size", "Glitter sparkle size", "A sparkle's diameter in model units (a stud is 0.8).", "0.1", 0.01f, 1));
		c.Add(Float(UGC, "glitter_sparkle_amount", "Glitter sparkle amount", "Percent of each moving sparkle layer covered by sparkles; a sparkle shows where two meet, so about this share squared of a brick sparkles at once.", "5", 0, 50));
		c.Add(Float(UGC, "glitter_speed", "Glitter sparkle speed", "How fast sparkles flash and go out: 1 is about half a second each (the sparkle texture is made bigger so the client's fixed layer motion crosses sparkles faster).", "1", 0.1f, 4));
		c.Add(Float(UGC, "glitter_sparkle_tint", "Glitter sparkle tint", "Percent: how far the sparkles take their brick's color (0: white).", "30", 0, 100));
		c.Add(Float(UGC, "glitter_sparkle_brightness", "Glitter sparkle brightness", "Percent: the sparkles' vertex color (the client lights them like its other surfaces).", "100", 0, 100));
		c.Add(Bool(UGC, "glitter_random", "Glitter placed per brick", "Each glitter brick gets its own fleck pattern (turned and moved by a number of the brick's own, the same every time the model is made); off: the same pattern on every brick.", true));
		c.Add(Text(UGC, "satin_colors", "Satin colors", "Satin (opal) color ids, comma separated: they stay transparent plastic (the client has no satin shader) but are made milky and less see-through. By default LEGO's satin colors 360,362,363,364,365,366,367,376 (none: off)." + notLive, "360,362,363,364,365,366,367,376"));
		c.Add(Float(UGC, "satin_opacity", "Satin opacity", "Percent: the opacity of transparent satin bricks, instead of the transparent opacity.", "75", 0, 100));
		c.Add(Float(UGC, "satin_whiten", "Satin whitening", "Percent: how far satin colors go towards white.", "20", 0, 100));
		c.Add(Bool(UGC, "remove_hidden_faces", "Remove faces nobody can see", "The opaque bricks are rendered from 42 directions around the model; the triangles that show in none are removed. Faces seen only through openings or by bounced light go too.", true));
		c.Add(Bool(UGC, "hsr_ground_plane", "Nothing seen from below", "Also removes what can only be seen from under the model.", false));
		c.Add(Unit(Int(UGC, "hsr_resolution", "Detail of the visibility renders", "The size of each of the 42 renders; bigger keeps smaller visible faces.", "1024", 64, 4096), "pixels"));
		c.Add(Labels(Choice(UGC, "ray_backend", "Ray tracer", "What traces the occlusion rays: embree (Intel Embree on the CPU), hiprt (AMD or NVIDIA GPUs through HIPRT) or embree-gpu (Intel Arc or Xe GPUs through Embree's SYCL), each GPU one only when the server was built with it and has such a GPU (else embree). They give the same results but for rounding. Staff can pick another for one make when making models again.", "embree", { "embree", "hiprt", "embree-gpu" }), { "Embree (CPU)", "HIPRT (AMD or NVIDIA GPU)", "Embree (Intel GPU)" }));
		c.Add(Int(UGC, "hiprt_device", "GPU for HIPRT", "Which GPU the hiprt ray tracer uses: 0 is the first HIP (AMD) or CUDA (NVIDIA) device.", "0", 0, 16, true));
		c.Add(Int(UGC, "embree_gpu_device", "Intel GPU for Embree", "Which Intel GPU the embree-gpu ray tracer uses: 0 is the first Embree supports.", "0", 0, 16, true));
		c.Add(Labels(Choice(UGC, "denoise", "Denoise icons", "oidn (when the server was built with Intel Open Image Denoise; else off): a model's icon is drawn from its colors before the occlusion bake, with its occlusion traced per pixel with a few rays and the noise removed by the denoiser, instead of the baked occlusion. The model itself keeps its baked occlusion (a denoiser only works on images).", "off", { "off", "oidn" }), { "Off", "Open Image Denoise" }));
		c.Add(Int(UGC, "denoise_samples", "Denoised occlusion rays per pixel", "Occlusion rays traced from each pixel of a denoised icon (before it is scaled down, so 16 times as many per icon pixel); fewer is faster and noisier.", "4", 1, 256));
		// LU Toolbox itself in a headless Blender (docs/UgcServer.md, "LU Toolbox in Blender")
		c.Add(Labels(Choice(UGC, "processor", "What makes the models", "native: the UGC server's own pipeline. toolbox-blender: LU Toolbox itself in a headless Blender (import, Process Model, Bake Lighting, niftools export), one model at a time and far slower; when Blender, LU Toolbox or niftools can't be found (the settings below), models are made natively and the UGC server logs why. Staff can pick either for one make when making models again.", "native", { "native", "toolbox-blender" }), { "UGC server (native)", "LU Toolbox in Blender" }));
		c.Add(Format(Text(UGC, "toolbox_blender", "Blender executable", "The Blender that runs LU Toolbox (3.1 is what LU Toolbox and LU-Toolbox-Standalone are made for). An external program: nothing of it is built into the servers.", ""), eFormat::PATH));
		c.Add(Format(Text(UGC, "toolbox_standalone_dir", "LU-Toolbox-Standalone folder", "The folder with lu_batch_driver.py, whose steps the worker runs.", ""), eFormat::PATH));
		c.Add(Format(Text(UGC, "toolbox_scripts_dir", "Blender add-ons folder", "A Blender scripts folder whose addons/ has lu_toolbox and io_scene_niftools (empty: the add-ons installed in Blender's own user folder).", ""), eFormat::PATH));
		c.Add(Format(Text(UGC, "toolbox_brickdb_dir", "LU Toolbox brick folder", "Made from the client's brickdb.zip and brickprimitives the first time (so LU Toolbox doesn't unpack into the client's res folder). Relative to the server binaries.", "toolbox-brickdb"), eFormat::PATH));
		c.Add(Format(Text(UGC, "toolbox_work_dir", "LU Toolbox work folder", "The model being made and Blender's log (blender.log). Relative to the server binaries.", "toolbox-work"), eFormat::PATH));
		c.Add(Labels(Choice(UGC, "toolbox_device", "Blender's bake device", "What Cycles bakes on (Process Model's hidden faces and Bake Lighting): the CPU, or an NVIDIA (CUDA, OptiX) or AMD (HIP) GPU; auto: Blender's own preferences.", "cpu", { "cpu", "cuda", "optix", "hip", "auto" }), { "CPU", "CUDA", "OptiX", "HIP", "Auto" }));
		c.Add(Int(UGC, "toolbox_threads", "Blender's threads", "CPU threads Blender uses. Its CPU time counts against the CPU limit, which pauses it; worker_nice applies to it.", "4", 1, 256));
		c.Add(Unit(Int(UGC, "toolbox_timeout_seconds", "LU Toolbox time limit", "A model taking longer fails and Blender is started again.", "1800", 10, 86400), "seconds"));
		c.Add(Bool(UGC, "bake_ao", "Darken hidden corners", "Ambient occlusion baked into the vertex colors.", true));
		c.Add(Int(UGC, "ao_samples", "Occlusion rays per vertex", "", "64", 1, 1024));
		c.Add(Float(UGC, "ao_distance", "Occlusion distance", "", "5", 0, 1000));
		c.Add(Float(UGC, "ao_strength", "Darkening strength", "0 to 1.", "1", 0, 1));
		c.Add(Float(UGC, "glow_strength", "Glow strength", "What glowing colors add to the baked light.", "6", 0, 100));
		c.AddSection("UGC storage", "The files made, kept on disk.", eLayout::ROWS, Condition{ MASTER, "enable_ugc_server", { "1" } });
		c.Add(Format(Text(UGC, "ugc_output_dir", "Files folder", "Relative to the server binaries.", "ugc", true), eFormat::PATH));
		c.Add(Unit(Int(UGC, "ugc_max_storage_mb", "Most space for files", "The files used longest ago are deleted past this and made again when asked for. 0: no limit.", "2048", 0, std::nullopt, true), "MB"));
		c.AddSection("UGC icons", "The icons' size and the defaults of their framing and light. Presets per type and values for single items are set in the UGC page's icon editor.",
			eLayout::ROWS, Condition{ MASTER, "enable_ugc_server", { "1" } });
		c.Add(Unit(Int(UGC, "icon_size", "Icon size", "", "128", 16, 1024), "pixels"));
		// The icon's framing and light, from the one list of them (UgcIconParams); presets and overrides are set on the
		// UGC page's icon editor
		for (const auto& param : UgcIconParams::List()) {
			char number[32];
			const auto written = std::to_chars(number, number + sizeof(number), param.defaultValue);
			auto setting = Float(UGC, param.setting, "Icon: " + param.label, param.description, std::string(number, written.ptr), param.min, param.max);
			if (!param.unit.empty()) setting = Unit(setting, param.unit);
			c.Add(setting);
		}

		return c;
	}

	const Catalog& Get() {
		static const auto catalog = Build();
		return catalog;
	}

	// Shortest form that reads back the same, e.g. 0.25 or 1000
	std::string Number(double value) {
		char buffer[64];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
		return std::string(buffer, result.ptr);
	}

	std::string Trim(std::string text) {
		text.erase(0, text.find_first_not_of(" \t\r\n"));
		text.erase(text.find_last_not_of(" \t\r\n") + 1);
		return text;
	}

	std::string Lower(std::string text) {
		for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return text;
	}

	std::string RangeText(const Setting& setting) {
		if (setting.min && setting.max) return " between " + Number(*setting.min) + " and " + Number(*setting.max);
		if (setting.min) return " of at least " + Number(*setting.min);
		if (setting.max) return " of at most " + Number(*setting.max);
		return "";
	}
}

namespace SettingsCatalog {
	const std::vector<Category>& Categories() { return Get().categories; }
	const std::vector<Section>& Sections() { return Get().sections; }
	const std::vector<Setting>& All() { return Get().settings; }

	const Section* FindSection(const std::string& name) {
		for (const auto& section : Sections()) if (section.name == name) return &section;
		return nullptr;
	}

	const Setting* Find(const std::string& file, const std::string& key) {
		static const auto index = [] {
			std::map<std::pair<std::string, std::string>, const Setting*> map;
			for (const auto& setting : All()) map[{ setting.file, setting.key }] = &setting;
			return map;
		}();
		const auto it = index.find({ file, key });
		return it == index.end() ? nullptr : it->second;
	}

	const std::vector<std::pair<std::string, std::string>>& Files() {
		static const std::vector<std::pair<std::string, std::string>> files{
			{ SHARED, "Shared" }, { WORLD, "World" }, { AUTH, "Auth" }, { CHAT, "Chat" }, { MASTER, "Master" }, { DASHBOARD, "Dashboard" }, { UGC, "UGC" }
		};
		return files;
	}

	std::string FormatName(eFormat format) {
		switch (format) {
		case eFormat::PLAIN: return "plain";
		case eFormat::PATH: return "path";
		case eFormat::HOST: return "host";
		case eFormat::URL: return "url";
		case eFormat::EMAIL: return "email";
		}
		return "plain";
	}

	std::string ListOfName(eListOf listOf) {
		switch (listOf) {
		case eListOf::NUMBER: return "number";
		case eListOf::ZONE: return "zone";
		case eListOf::LOT: return "lot";
		case eListOf::REWARD_CODE: return "reward_code";
		}
		return "number";
	}

	std::string LayoutName(eLayout layout) {
		switch (layout) {
		case eLayout::ROWS: return "rows";
		case eLayout::GRID: return "grid";
		case eLayout::PAIRS: return "pairs";
		}
		return "rows";
	}

	std::string TypeName(eType type) {
		switch (type) {
		case eType::BOOL: return "bool";
		case eType::INT: return "int";
		case eType::FLOAT: return "float";
		case eType::TEXT: return "text";
		case eType::SECRET: return "secret";
		case eType::INT_LIST: return "int_list";
		case eType::CHOICE: return "choice";
		}
		return "text";
	}

	std::optional<std::string> Normalize(const Setting& setting, const std::string& input, std::string& error) {
		auto value = setting.multiline ? input : Trim(input);
		if (value.empty()) return value;
		if (!setting.multiline && value.find_first_of("\r\n") != std::string::npos) {
			error = "Use one line";
			return std::nullopt;
		}

		switch (setting.type) {
		case eType::BOOL: {
			const auto lower = Lower(value);
			if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") return "1";
			if (lower == "0" || lower == "false" || lower == "no" || lower == "off") return "0";
			error = "Use on or off (1 or 0)";
			return std::nullopt;
		}
		case eType::INT: {
			const auto parsed = GeneralUtils::TryParse<int64_t>(value);
			if (!parsed || (setting.min && *parsed < *setting.min) || (setting.max && *parsed > *setting.max)) {
				error = "Enter a whole number" + RangeText(setting);
				return std::nullopt;
			}
			return std::to_string(*parsed);
		}
		case eType::FLOAT: {
			const auto parsed = GeneralUtils::TryParse<double>(value);
			if (!parsed || !std::isfinite(*parsed) || (setting.min && *parsed < *setting.min) || (setting.max && *parsed > *setting.max)) {
				error = "Enter a number" + RangeText(setting);
				return std::nullopt;
			}
			return Number(*parsed);
		}
		case eType::INT_LIST: {
			std::string normalized;
			for (auto part : GeneralUtils::SplitString(value, ',')) {
				part = Trim(part);
				if (part.empty()) continue;
				const auto parsed = GeneralUtils::TryParse<int64_t>(part);
				if (!parsed || *parsed < 0) {
					error = "\"" + part + "\" is not a whole number; use numbers separated by commas";
					return std::nullopt;
				}
				if (!normalized.empty()) normalized += ",";
				normalized += std::to_string(*parsed);
			}
			return normalized;
		}
		case eType::CHOICE: {
			for (const auto& choice : setting.choices) {
				if (Lower(value) == Lower(choice)) return choice;
			}
			std::string list;
			for (const auto& choice : setting.choices) list += (list.empty() ? "" : ", ") + choice;
			error = "Pick one of: " + list;
			return std::nullopt;
		}
		case eType::TEXT:
		case eType::SECRET:
			if (value.size() > 4000) {
				error = "Too long (4000 characters at most)";
				return std::nullopt;
			}
			if (setting.format == eFormat::URL && !value.starts_with("http://") && !value.starts_with("https://")) {
				error = "Enter a web address starting with http:// or https://";
				return std::nullopt;
			}
			if (setting.format == eFormat::EMAIL && (value.find('@') == std::string::npos || value.find_first_of(" \t,;") != std::string::npos)) {
				error = "Enter one email address";
				return std::nullopt;
			}
			if (setting.format == eFormat::HOST && value.find_first_of(" \t/") != std::string::npos) {
				error = "Enter a host name or address, without spaces or slashes";
				return std::nullopt;
			}
			return value;
		}
		return value;
	}
}
