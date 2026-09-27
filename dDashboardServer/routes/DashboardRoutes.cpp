#include "DashboardRoutes.h"
#include "RouteUtils.h"
#include "Permissions.h"
#include "EmailService.h"
#include "PasswordRecovery.h"
#include "BinaryPathFinder.h"
#include "ePermissionMap.h"
#include "eObjectBits.h"
#include "ServerState.h"
#include "Web.h"
#include "HTTPContext.h"
#include "eHTTPMethod.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"
#include "Locale.h"
#include "GeneralUtils.h"
#include "CDClientDatabase.h"
#include "tinyxml2.h"
#include <set>
#include "ReportRoutes.h"
#include "GameLabels.h"
#include "BehaviorXml.h"
#include "ClientAssets.h"
#include <chrono>
#include <algorithm>
#include <fstream>
#include <filesystem>

using namespace RouteUtils;

namespace {
	std::string ZoneName(int zoneId) {
		if (zoneId == 0) return "Character Select";
		const auto& name = Locale::GetPhrase("ZoneTable_" + std::to_string(zoneId) + "_DisplayDescription");
		return name.empty() ? "Zone " + std::to_string(zoneId) : name;
	}

	// Built once (worker threads read it too, so never lazily twice)
	const nlohmann::json& GetZoneNamesJson() {
		static const nlohmann::json names = [] {
			nlohmann::json names;
			names["0"] = "Character Select";
			for (const auto& key : Locale::GetPhraseIdsWithPrefix("ZoneTable_")) {
				if (key.find("_DisplayDescription") == std::string::npos) continue;
				const auto start = std::string("ZoneTable_").length();
				const auto end = key.find("_DisplayDescription");
				const auto& name = Locale::GetPhrase(key);
				if (!name.empty()) names[key.substr(start, end - start)] = name;
			}
			return names;
		}();
		return names;
	}

	nlohmann::json ParseCharacterXml(const std::string& xml) {
		// Every value the template reads has a default, since inja fails on missing variables and
		// character XML may lack sections (e.g. a character that never finished loading in)
		nlohmann::json stats{
			{"coins", 0}, {"universe_score", 0}, {"reputation", 0}, {"gm_level", 0}, {"level", 0},
			{"last_zone_id", 0}, {"last_zone", ZoneName(0)},
			{"health", 0}, {"max_health", 0}, {"armor", 0}, {"max_armor", 0}, {"imagination", 0}, {"max_imagination", 0},
			{"missions_completed", 0}, {"missions_active", 0}, {"total_items", 0}
		};
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return stats;

		auto* obj = doc.FirstChildElement("obj");
		if (!obj) return stats;

		auto* charEl = obj->FirstChildElement("char");
		if (charEl) {
			stats["coins"] = charEl->Int64Attribute("cc", 0);
			stats["universe_score"] = charEl->Int64Attribute("ls", 0);
			stats["reputation"] = charEl->Int64Attribute("rpt", 0);
			stats["gm_level"] = charEl->IntAttribute("gm", 0);

			auto lastZone = charEl->IntAttribute("lwid", 0);
			stats["last_zone_id"] = lastZone;
			stats["last_zone"] = ZoneName(lastZone);
		}

		auto* lvlEl = obj->FirstChildElement("lvl");
		if (lvlEl) {
			stats["level"] = lvlEl->IntAttribute("l", 0);
		}

		auto* mf = obj->FirstChildElement("mf");
		if (mf) {
			stats["shirt_color"] = mf->IntAttribute("hc", 0);
			stats["hair_style"] = mf->IntAttribute("hs", 0);
			stats["hair_color"] = mf->IntAttribute("hd", 0);
			stats["shirt_style"] = mf->IntAttribute("t", 0);
			stats["pants_color"] = mf->IntAttribute("l", 0);
			stats["eyebrow_style"] = mf->IntAttribute("es", 0);
			stats["eyes_style"] = mf->IntAttribute("ess", 0);
			stats["mouth_style"] = mf->IntAttribute("ms", 0);
		}

		auto* dest = obj->FirstChildElement("dest");
		if (dest) {
			stats["max_health"] = dest->IntAttribute("hm", 0);
			stats["health"] = dest->IntAttribute("hc", 0);
			stats["max_imagination"] = dest->IntAttribute("im", 0);
			stats["imagination"] = dest->IntAttribute("ic", 0);
			stats["max_armor"] = dest->IntAttribute("am", 0);
			stats["armor"] = dest->IntAttribute("ac", 0);
		}

		if (charEl) {
			auto* vl = charEl->FirstChildElement("vl");
			if (vl) {
				// One entry per zone and clone (each property visited is its own clone): list each zone once, with how
				// many of its clones were visited
				std::map<int, std::set<int>> clones;
				for (auto* l = vl->FirstChildElement("l"); l; l = l->NextSiblingElement("l")) clones[l->IntAttribute("id", 0)].insert(l->IntAttribute("cid", 0));
				nlohmann::json visited = nlohmann::json::array();
				for (const auto& [zid, cids] : clones) visited.push_back({ {"id", zid}, {"name", ZoneName(zid)}, {"clones", cids.size()} });
				stats["visited_zones"] = visited;
			}

			auto* zs = charEl->FirstChildElement("zs");
			if (zs) {
				nlohmann::json zoneStats = nlohmann::json::array();
				for (auto* s = zs->FirstChildElement("s"); s; s = s->NextSiblingElement("s")) {
					auto zid = s->IntAttribute("map", 0);
					zoneStats.push_back({
						{"map_id", zid}, {"name", ZoneName(zid)},
						{"achievements", s->IntAttribute("ac", 0)},
						{"coins_collected", s->IntAttribute("cc", 0)},
						{"enemies_smashed", s->IntAttribute("es", 0)}
					});
				}
				stats["zone_stats"] = zoneStats;
			}
		}

		auto* mis = obj->FirstChildElement("mis");
		if (mis) {
			auto* done = mis->FirstChildElement("done");
			auto* cur = mis->FirstChildElement("cur");
			int completedCount = 0, activeCount = 0;
			if (done) for (auto* m = done->FirstChildElement("m"); m; m = m->NextSiblingElement("m")) completedCount++;
			if (cur) for (auto* m = cur->FirstChildElement("m"); m; m = m->NextSiblingElement("m")) activeCount++;
			stats["missions_completed"] = completedCount;
			stats["missions_active"] = activeCount;
		}

		auto* inv = obj->FirstChildElement("inv");
		if (inv) {
			auto* items = inv->FirstChildElement("items");
			if (items) {
				// The inventories worth showing, in this order; named from the enum (GameLabels.h)
				const eInventoryType standardTabs[] = { ITEMS, VAULT_ITEMS, BRICKS, MODELS, VAULT_MODELS, BEHAVIORS, QUEST };

				std::map<int, nlohmann::json> invItemsByType;
				std::set<int> allLots;
				int totalItems = 0;

				for (auto* invType = items->FirstChildElement("in"); invType; invType = invType->NextSiblingElement("in")) {
					int typeId = invType->IntAttribute("t", -1);
					nlohmann::json invItems = nlohmann::json::array();
					for (auto* item = invType->FirstChildElement("i"); item; item = item->NextSiblingElement("i")) {
						int lot = item->IntAttribute("l", 0);
						allLots.insert(lot);
						invItems.push_back({
							{"id", item->Attribute("id") ? item->Attribute("id") : ""},
							{"lot", lot},
							{"count", item->IntAttribute("c", 1)},
							{"slot", item->IntAttribute("s", 0)},
							{"equipped", std::string(item->Attribute("eq") ? item->Attribute("eq") : "false") == "true"},
							{"bound", std::string(item->Attribute("b") ? item->Attribute("b") : "false") == "true"}
						});
						totalItems++;
					}
					invItemsByType[typeId] = invItems;
				}

				std::unordered_map<int, std::string> lotNames;
				for (const auto lot : allLots) {
					if (auto name = ClientAssets::ObjectName(lot)) lotNames[lot] = std::move(*name);
				}

				nlohmann::json inventories = nlohmann::json::array();
				for (const auto& tab : standardTabs) {
					nlohmann::json tabItems = nlohmann::json::array();
					auto it = invItemsByType.find(static_cast<int>(tab));
					if (it != invItemsByType.end()) tabItems = it->second;

					for (auto& item : tabItems) {
						int lot = item["lot"].get<int>();
						auto nameIt = lotNames.find(lot);
						item["name"] = (nameIt != lotNames.end()) ? nameIt->second : ("LOT " + std::to_string(lot));
					}

					inventories.push_back({{"type_id", static_cast<int>(tab)}, {"type_name", GameLabels::Name(tab)}, {"items", tabItems}});
				}

				stats["inventories"] = inventories;
				stats["total_items"] = totalItems;
			}
		}

		return stats;
	}
}

const nlohmann::json& ZoneNames() {
	return GetZoneNamesJson();
}

namespace {
	constexpr size_t LOG_TAIL_BYTES = 256 * 1024;
	constexpr size_t LOG_TAIL_LINES = 500;

	// Register a page that only needs the current user and navigation state
	template<typename Access>
	void SimplePage(const std::string& path, const Access& access, const std::string& templateName, const std::string& page, const std::string& description) {
		Route(eHTTPMethod::GET, path, access, description, [templateName, page](HTTPReply& reply, const HTTPContext& context) {
			RenderPage(reply, context, templateName, page);
		});
	}

	struct LogFile {
		std::filesystem::path path;
		int64_t time{}; // last written, Unix seconds
		uintmax_t size{};
	};

	// Every log file, by server (the name before the start time, e.g. "WorldServer_1200_0_3"), newest first. Servers
	// write into their own folders (logs/WorldServer/<zone>/<clone>/, logs/AuthServer/, ...), so the whole tree is read.
	std::map<std::string, std::vector<LogFile>> LogFilesByServer() {
		std::map<std::string, std::vector<LogFile>> byServer;
		const auto logDir = BinaryPathFinder::GetBinaryDir() / "logs";
		std::error_code ec;
		for (auto it = std::filesystem::recursive_directory_iterator(logDir, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
			const auto& entry = *it;
			if (!entry.is_regular_file(ec) || entry.path().extension() != ".log") continue;
			const auto stem = entry.path().stem().string();
			const auto written = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::clock_cast<std::chrono::system_clock>(entry.last_write_time(ec)).time_since_epoch()).count();
			byServer[stem.substr(0, stem.rfind('_'))].push_back({ entry.path(), written, entry.file_size(ec) });
		}
		for (auto& [server, files] : byServer) std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.time > b.time; });
		return byServer;
	}

	// Read the last lines of a file without loading all of it
	std::string TailFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return "Unable to open log file";
		const auto size = static_cast<size_t>(file.tellg());
		const size_t start = size > LOG_TAIL_BYTES ? size - LOG_TAIL_BYTES : 0;
		file.seekg(static_cast<std::streamoff>(start));
		std::string content(size - start, '\0');
		file.read(content.data(), static_cast<std::streamsize>(content.size()));

		size_t pos = content.size();
		for (size_t lines = 0; pos > 0 && lines <= LOG_TAIL_LINES; lines++) {
			pos = content.rfind('\n', pos - 1);
			if (pos == std::string::npos) { pos = 0; break; }
		}
		return content.substr(pos == 0 ? 0 : pos + 1);
	}
}

void RegisterDashboardRoutes() {
	Route(eHTTPMethod::GET, "/", 0, "Dashboard home", [](HTTPReply& reply, const HTTPContext& context) {
		nlohmann::json data = Can(context, "players_view") ? ServerState::GetServerStateJson() : ServerState::PlayerSafe(ServerState::GetServerStateJson());
		data["my_characters"] = Database::Get()->GetAccountCharacters(context.accountId);
		data["stats"]["totalAccounts"] = Database::Get()->GetAccountCount();
		data["stats"]["totalCharacters"] = Database::Get()->GetCharacterCount();
		RenderPage(reply, context, "index.jinja2", "home", data);
	});

	Route(eHTTPMethod::GET, "/login", PUBLIC, "Login page", [](HTTPReply& reply, const HTTPContext& context) {
		if (context.isAuthenticated) {
			reply.status = eHTTPStatusCode::FOUND;
			reply.location = "/";
			reply.message = "";
			return;
		}
		RenderPage(reply, context, "login.jinja2", "login");
	});

	Route(eHTTPMethod::GET, "/register", PUBLIC, "Account registration (allow_registration=1)", [](HTTPReply& reply, const HTTPContext& context) {
		if (!ConfigFlag("allow_registration", false)) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Registration is disabled on this server.");
		RenderPage(reply, context, "register.jinja2", "register", { {"play_key_required", ConfigFlag("registration_requires_play_key", true)} });
	});

	// Password reset and email confirmation are reached from emailed links, signed in or not
	// Either way of choosing a new password: an emailed link, or two-factor login's recovery codes (PasswordRecovery.cpp)
	Route(eHTTPMethod::GET, "/forgot_password", PUBLIC, "Reset a forgotten password by email or with two-factor recovery codes", [](HTTPReply& reply, const HTTPContext& context) {
		const bool email = EmailService::IsConfigured();
		const bool recovery = RecoveryResetEnabled();
		if (!email && !recovery) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Password reset is not available on this server. Ask an administrator.");
		RenderPage(reply, context, "forgot_password.jinja2", "forgot_password", { {"email_enabled", email}, {"recovery_enabled", recovery} });
	});
	SimplePage("/reset_password", PUBLIC, "reset_password.jinja2", "reset_password", "Choose a new password from an emailed link");
	// Public because the provider's redirect is cross-site, so the SameSite session cookie isn't sent;
	// the page finishes the flow with a same-origin request that does carry it
	SimplePage("/oauth2/callback", PUBLIC, "oauth2_callback.jinja2", "oauth2_callback", "Return from connecting a mail account");
	SimplePage("/verify_email", PUBLIC, "verify_email.jinja2", "verify_email", "Confirm an email address from an emailed link");

	// Accounts
	SimplePage("/accounts", Perm("accounts_view"), "accounts.jinja2", "accounts", "Accounts list");

	Route(eHTTPMethod::GET, "/account", 0, "Your own account", [](HTTPReply& reply, const HTTPContext& context) {
		reply.status = eHTTPStatusCode::FOUND;
		reply.location = "/accounts/" + std::to_string(context.accountId);
		reply.message = "";
	});

	Route(eHTTPMethod::GET, "/accounts/:id", 0, "Account details", [](HTTPReply& reply, const HTTPContext& context) {
		const auto accountId = PathId<uint32_t>(context.path, 1);
		if (!accountId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid account ID");
		if (!Can(context, "accounts_view") && context.accountId != *accountId) {
			return RenderError(reply, context, eHTTPStatusCode::FORBIDDEN, "You do not have permission to view this account.");
		}

		auto account = Database::Get()->GetAccountById(*accountId);
		if (account.contains("error")) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Account not found");
		account["characters"] = Database::Get()->GetAccountCharacters(*accountId);
		const auto email = Database::Get()->GetAccountEmail(*accountId);
		account["email"] = email ? email->email : "";
		account["email_confirmed"] = email && email->confirmed;
		account["two_factor"] = Database::Get()->GetTotp(*accountId).enabledAt != 0;
		// The play key this account registered with (key strings only with play_keys_manage)
		account["play_key"] = nullptr;
		if (const auto info = Database::Get()->GetAccountInfo(account.value("name", std::string{})); info && info->playKeyId > 0) {
			const auto key = Database::Get()->GetPlayKey(static_cast<int32_t>(info->playKeyId));
			account["play_key"] = { {"id", info->playKeyId}, {"key_string", Can(context, "play_keys_manage") && !key.contains("error") ? key.value("key_string", "") : ""} };
		}

		const uint8_t targetLevel = account.value("gm_level", 0);
		// Which of the viewer's tools work on this account (self_*, manage_equal_rank; always all of them for GM 9)
		const auto manage = ManageJson(context, targetLevel, *accountId);
		RenderPage(reply, context, "account-view.jinja2", "accounts", {
			{"account", account},
			{"is_self", context.accountId == *accountId},
			{"manage", manage},
			{"can_manage", manage["moderation"]},
			{"email_enabled", EmailService::IsConfigured()}
		});
	});

	// Characters
	SimplePage("/characters", Perm("characters_view"), "characters.jinja2", "characters", "Characters list");

	Route(eHTTPMethod::GET, "/characters/:id", 0, "Character details. Players may view their own characters", [](HTTPReply& reply, const HTTPContext& context) {
		const auto charId = PathId<LWOOBJID>(context.path, 1);
		if (!charId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid character ID");

		auto character = Database::Get()->GetCharacterById(*charId);
		if (character.contains("error")) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Character not found");
		const uint32_t ownerAccountId = character.value("account_id", 0u);
		if (!CanViewCharacter(context, ownerAccountId)) {
			return RenderError(reply, context, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters.");
		}
		character["is_own"] = ownerAccountId == context.accountId;
		const auto owner = Database::Get()->GetAccountById(ownerAccountId);
		character["manage"] = ManageJson(context, owner.contains("error") ? 0 : owner.value("gm_level", 0), ownerAccountId);

		const auto xml = Database::Get()->GetCharacterXml(*charId);
		if (!xml.empty()) character["stats"] = ParseCharacterXml(xml);

		const uint64_t permissions = character.value("permission_map", 0);
		character["restrictions"] = {
			{"trade", (permissions & static_cast<uint64_t>(ePermissionMap::RestrictedTradeAccess)) != 0},
			{"mail", (permissions & static_cast<uint64_t>(ePermissionMap::RestrictedMailAccess)) != 0},
			{"chat", (permissions & static_cast<uint64_t>(ePermissionMap::RestrictedChatAccess)) != 0}
		};
		RenderPage(reply, context, "character-view.jinja2", "characters", {
			{"character", character},
			{"xml_upload_enabled", ConfigFlag("enable_char_xml_upload", false)}
		});
	});

	// Content
	SimplePage("/play_keys", Perm("play_keys_manage"), "play_keys.jinja2", "play_keys", "Play keys");

	Route(eHTTPMethod::GET, "/play_keys/:id", Perm("play_keys_manage"), "Play key details", [](HTTPReply& reply, const HTTPContext& context) {
		const auto keyId = PathId<int32_t>(context.path, 1);
		if (!keyId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid play key ID");
		const auto key = Database::Get()->GetPlayKey(*keyId);
		if (key.contains("error")) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Play key not found");
		RenderPage(reply, context, "play_key-view.jinja2", "play_keys", { {"key", key} });
	});

	SimplePage("/properties", Perm("properties_view"), "properties.jinja2", "properties", "Properties");

	// Data is loaded by the page from /api/properties/:id, which enforces that players only see their own
	Route(eHTTPMethod::GET, "/properties/:id", 0, "Property details and 3D model viewer", [](HTTPReply& reply, const HTTPContext& context) {
		const auto propertyId = PathId<LWOOBJID>(context.path, 1);
		if (!propertyId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid property ID");
		RenderPage(reply, context, "property-view.jinja2", "properties", { {"property_id", std::to_string(*propertyId)} });
	});
	Route(eHTTPMethod::GET, "/properties/:id/3d", 0, "3D view of a property: models, terrain and behaviors", [](HTTPReply& reply, const HTTPContext& context) {
		const auto propertyId = PathId<LWOOBJID>(context.path, 1);
		if (!propertyId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid property ID");
		// The behavior blocks the server plays, so the viewer's behavior player plays the same ones
		static const auto behaviorRules = BehaviorXml::Rules(ClientAssets::ItemName).dump();
		RenderPage(reply, context, "property-3d.jinja2", "properties", { {"property_id", std::to_string(*propertyId)}, {"behavior_rules", behaviorRules} });
	});
	SimplePage("/bug_reports", Perm("bug_reports_view"), "bug_reports.jinja2", "bug_reports", "Bug reports");

	Route(eHTTPMethod::GET, "/bug_reports/:id", Perm("bug_reports_view"), "Bug report details", [](HTTPReply& reply, const HTTPContext& context) {
		const auto reportId = PathId<uint32_t>(context.path, 1);
		if (!reportId) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Invalid bug report ID");
		auto report = Database::Get()->GetBugReport(*reportId);
		if (report.contains("error")) return RenderError(reply, context, eHTTPStatusCode::NOT_FOUND, "Bug report not found");

		// The client sends the other player's object id, which is their character id. Reports about another player now
		// become player reports (PlayerReports::ReportPlayer); older bug reports may still name one here.
		report["other_player_name"] = "";
		report["other_player_char_id"] = "";
		if (const auto otherId = GeneralUtils::TryParse<LWOOBJID>(report.value("other_player_id", std::string{})); otherId && *otherId != 0) {
			if (const auto info = Database::Get()->GetCharacterInfo(*otherId)) {
				report["other_player_name"] = info->name;
				report["other_player_char_id"] = std::to_string(info->id);
			}
		}
		// Selections are locale keys wrapped like %[UI_BUG_REPORT_...]
		const std::string selection = report.value("selection", std::string{});
		report["selection_text"] = selection;
		if (selection.starts_with("%[") && selection.ends_with("]")) {
			const auto& phrase = Locale::GetPhrase(selection.substr(2, selection.size() - 3));
			if (!phrase.empty()) report["selection_text"] = phrase;
		}
		RenderPage(reply, context, "bug_report-view.jinja2", "bug_reports", { {"report", report} });
	});

	// Moderation and tools
	// Holds three queues; anyone who may work one of them gets in and sees only theirs
	Route(eHTTPMethod::GET, "/moderation", 1, "Moderation queue (names, pet names, properties)", [](HTTPReply& reply, const HTTPContext& context) {
		if (!Can(context, "moderate_names") && !Can(context, "moderate_pet_names") && !Can(context, "moderate_properties")) {
			return RenderError(reply, context, eHTTPStatusCode::FORBIDDEN, "You don't have permission to review names or properties.");
		}
		RenderPage(reply, context, "moderation.jinja2", "moderation");
	});
	SimplePage("/reports", Perm("reports_view"), "reports.jinja2", "reports", "Economy and world reports");
	SimplePage("/pet_names", Perm("moderate_pet_names"), "pet_names.jinja2", "pet_names", "Pet names");
	SimplePage("/send_mail", Perm("mail_send"), "send_mail.jinja2", "send_mail", "Send mail");
	SimplePage("/api_docs", 0, "api_docs.jinja2", "api_docs", "API documentation");
	SimplePage("/maintenance", Perm("maintenance"), "maintenance.jinja2", "maintenance", "Data maintenance tools");
	SimplePage("/webhooks", Perm("webhooks"), "webhooks.jinja2", "webhooks", "Outgoing webhooks for alerts");
	SimplePage("/tasks", Perm("tasks_view"), "tasks.jinja2", "tasks", "Scheduled tasks: schedules, runs and logs");
	Route(eHTTPMethod::GET, "/api/zones", Perm("characters_rescue"), "Every zone with its name, for pickers: {zones: [{id, name}]}", [](HTTPReply& reply, const HTTPContext&) {
		nlohmann::json zones = nlohmann::json::array();
		for (const auto& [id, name] : GetZoneNamesJson().items()) {
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(id); zone && *zone > 0) zones.push_back({ {"id", *zone}, {"name", name} });
		}
		std::sort(zones.begin(), zones.end(), [](const auto& a, const auto& b) { return a["id"].template get<uint32_t>() < b["id"].template get<uint32_t>(); });
		JsonSuccess(reply, { {"zones", zones} });
	});
	Route(eHTTPMethod::GET, "/api/zones/:id/spawn_points", Perm("characters_rescue"),
		"Where in a zone a rescued player can land (the named spawn points in its scene files, as rocket launchers use): {spawnPoints: [{name, lot, x, y, z}]}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto zoneId = PathId<uint32_t>(context.path, 2);
			if (!zoneId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid zone");
			const auto points = ZoneSpawnPointsJson(*zoneId);
			if (!points) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Unknown zone, or the server can't read the client's files (client_location)");
			JsonSuccess(reply, { {"spawnPoints", *points} });
		});
	SimplePage("/chat_log", Perm("chat_view"), "chat_log.jinja2", "chat_log", "The chat log");
	SimplePage("/vanity", Perm("vanity_manage"), "vanity.jinja2", "vanity", "Vanity: the vanity files and their NPCs, plaque texts, vanity events and a preview of what the worlds load");
	SimplePage("/leaderboards", Perm("leaderboards_view"), "leaderboards.jinja2", "leaderboards", "Leaderboards for every activity");
	SimplePage("/health", Perm("health_view"), "health.jinja2", "health", "Player counts, worlds, uptime and memory over time; crash dumps");
	SimplePage("/diagnostics", Perm("health_view"), "diagnostics.jinja2", "diagnostics", "Packets, bytes and HTTP requests per second of every server");
	SimplePage("/players", Perm("players_view"), "players.jinja2", "players", "Who is online, with kick, rescue and teleport");
	SimplePage("/backups", Perm("backups"), "backups.jinja2", "backups", "Database backups");
	SimplePage("/permissions", Perm("permissions_manage"), "permissions.jinja2", "permissions", "What each GM level may do");
	SimplePage("/settings", Perm("settings"), "settings.jinja2", "settings", "Server settings");
	SimplePage("/client_assets", Perm("client_files"), "client_assets.jinja2", "client_assets", "Browse the game client's files");
	Route(eHTTPMethod::GET, "/about", 0, "About this server", [](HTTPReply& reply, const HTTPContext& context) {
		RenderPage(reply, context, "about.jinja2", "about", { {"version", PROJECT_VERSION} });
	});

	// Logs
	Route(eHTTPMethod::GET, "/activity_log", Perm("logs_activity"), "Activity log", [](HTTPReply& reply, const HTTPContext& context) {
		RenderPage(reply, context, "activity_log.jinja2", "activity_log", { {"zone_names", GetZoneNamesJson()} });
	});
	SimplePage("/command_log", Perm("logs_command"), "command_log.jinja2", "command_log", "Command log");
	SimplePage("/audit_log", Perm("logs_audit"), "audit_log.jinja2", "audit_log", "Audit log");

	Route(eHTTPMethod::GET, "/system_log", Perm("logs_system"), "Server log files: the end of one, picked by server and file", [](HTTPReply& reply, const HTTPContext& context) {
		const auto logs = LogFilesByServer();
		// Only names from the directory listing are accepted, so the parameters can't select arbitrary files
		std::string selected = QueryValue(context.queryString, "server");
		if (!logs.contains(selected)) selected = logs.contains("MasterServer") ? "MasterServer" : (logs.empty() ? "" : logs.begin()->first);

		nlohmann::json servers = nlohmann::json::array();
		std::set<std::string> types; // process names for the search, e.g. "WorldServer"
		for (const auto& [server, files] : logs) {
			servers.push_back({ {"name", server}, {"time", files.front().time}, {"files", files.size()} });
			types.insert(server.substr(0, server.find('_')));
		}
		// World zones with log files, for Download logs
		std::set<uint32_t> zoneIds;
		for (const auto& [server, files] : logs) {
			if (!server.starts_with("WorldServer_")) continue;
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(server.substr(12, server.find('_', 12) - 12))) zoneIds.insert(*zone);
		}
		nlohmann::json zones = nlohmann::json::array();
		for (const auto zone : zoneIds) zones.push_back({ {"id", zone}, {"name", GetZoneNamesJson().value(std::to_string(zone), "")} });
		nlohmann::json data{ {"servers", servers}, {"server_types", types}, {"selected_server", selected}, {"files", nlohmann::json::array()}, {"bundle_zones", zones} };
		if (!selected.empty()) {
			const auto& files = logs.at(selected);
			const auto wanted = QueryValue(context.queryString, "file");
			const auto it = std::find_if(files.begin(), files.end(), [&](const LogFile& f) { return f.path.filename().string() == wanted; });
			const auto& shown = it != files.end() ? *it : files.front();
			for (const auto& file : files) data["files"].push_back({ {"name", file.path.filename().string()}, {"time", file.time}, {"size", file.size} });
			data["log_file"] = shown.path.filename().string();
			data["log_time"] = shown.time;
			data["log_size"] = shown.size;
			data["log_newest"] = &shown == &files.front();
			data["log_content"] = TailFile(shown.path);
		}
		RenderPage(reply, context, "system_log.jinja2", "system_log", data);
	});

	Route(eHTTPMethod::GET, "/api/logs/file", Perm("logs_system"), "Download a whole server log file. Query: ?name= (a file from the System Log page)",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto name = QueryValue(context.queryString, "name");
			for (const auto& [server, files] : LogFilesByServer()) {
				for (const auto& file : files) {
					if (file.path.filename().string() != name) continue;
					reply.file = file.path.string(); // streamed from disk
					reply.message.clear();
					reply.status = eHTTPStatusCode::OK;
					reply.contentType = eContentType::TEXT_PLAIN;
					reply.headers.push_back("Content-Disposition: attachment; filename=\"" + name + "\"");
					return;
				}
			}
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "No such log file");
		});
}
