#include "APIRoutes.h"
#include "PropertyAssets.h"
#include "OpenApi.h"
#include "RouteUtils.h"
#include "ApiKeyService.h"
#include "Permissions.h"
#include "DashboardAuthService.h"
#include "DashboardRoutes.h"
#include "MaintenanceRoutes.h"
#include "ReportRoutes.h"
#include "Strikes.h"
#include "AccountModeration.h"
#include "BehaviorXml.h"
#include "CharacterTools.h"
#include "ClientAssets.h"
#include "CharacterXml.h"
#include "CharacterXmlCheck.h"
#include "Scenery.h"
#include "Workers.h"
#include "LiveWorld.h"
#include "ServerState.h"
#include "WSRoutes.h"
#include "PlayerActions.h"
#include "AuthTokenHandler.h"
#include "JWTUtils.h"
#include "eServerDisconnectIdentifiers.h"
#include "Web.h"
#include "eHTTPMethod.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"
#include "HTTPContext.h"
#include "eGameMasterLevel.h"
#include "ePermissionMap.h"
#include "CDClientDatabase.h"
#include "dConfig.h"
#include "MailInfo.h"
#include "tinyxml2.h"
#include "Sd0.h"
#include <sstream>
#include <map>
#include "Locale.h"
#include <bcrypt/BCrypt.hpp>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>

using namespace RouteUtils;

namespace {
	// Tell the player in game about a moderation decision. The reply carries the request id, so the dashboard
	// shows whether they were online to hear it.
	nlohmann::json NotifyModeration(PlayerActionRequest request, uint32_t requester) {
		const auto id = PlayerActions::Request(request, requester, [](const PlayerActionResult& result) -> PlayerActions::Outcome {
			return { true, result.affected ? "The player was told in game." : "The player isn't online; they'll see the result next time they play." };
		});
		return { {"requestId", id} };
	}

	constexpr uint32_t MAX_PLAY_KEYS_PER_REQUEST = 100;
	constexpr int16_t MAX_MAIL_ATTACHMENT_COUNT = 999;

	using TableFetcher = std::function<std::string(const DataTablesRequest&, const nlohmann::json& body)>;

	// A placed model's behaviors (up to 5), parsed for the viewer
	nlohmann::json ModelBehaviors(const IPropertyContents::Model& model) {
		nlohmann::json list = nlohmann::json::array();
		for (const auto behaviorId : model.behaviors) {
			if (behaviorId == 0) continue;
			auto parsed = BehaviorXml::Parse(Database::Get()->GetBehavior(behaviorId));
			if (parsed.is_null()) continue;
			parsed["id"] = std::to_string(behaviorId);
			list.push_back(std::move(parsed));
		}
		return list;
	}

	// Table rows with zone_name next to zone_id
	std::string WithZoneNames(const std::string& table) {
		auto json = nlohmann::json::parse(table, nullptr, false);
		if (json.is_discarded()) return table;
		const auto& zones = ZoneNames();
		for (auto& row : json["data"]) {
			const auto zone = std::to_string(row.value("zone_id", 0));
			row["zone_name"] = zones.contains(zone) ? zones[zone].get<std::string>() : "Zone " + zone;
		}
		return json.dump();
	}

	/**
	 * Adds each pet's kind (its CDClient name) to pet_names rows, from the pet_lot the game writes (lot 0: not known yet).
	 */
	std::string WithPetKinds(const std::string& raw) {
		auto json = nlohmann::json::parse(raw, nullptr, false);
		if (json.is_discarded() || !json.contains("data") || !json["data"].is_array()) return raw;
		for (auto& row : json["data"]) {
			const LOT lot = row.value("lot", 0);
			row["kind"] = lot > 0 ? ClientAssets::ItemName(lot) : "";
		}
		return json.dump();
	}

	// Whether the request's API key (if any) may use a documented route: its scope, read-only and session-only paths
	bool KeyMayUse(const HTTPContext& context, const RouteDoc& doc) {
		if (!context.apiKey) return true;
		const auto& key = *context.apiKey;
		if (ApiKeyService::SessionOnlyPath(doc.path)) return false;
		if (key.readOnly && doc.method != "GET") return false;
		return doc.permission.empty() ? (doc.minGmLevel <= 0 || key.allPermissions) : key.Has(doc.permission);
	}

	// Register a DataTables endpoint. The fetcher returns the DB layer's JSON string. Access: a GM level or a Perm.
	template<typename Access>
	void TableRoute(const std::string& path, const Access& access, const std::string& description, TableFetcher fetcher) {
		ReadRoutes reads; // the query is a POST body, but it only reads
		Route(eHTTPMethod::POST, path, access, description, [fetcher = std::move(fetcher)](HTTPReply& reply, const HTTPContext& context) {
			const auto request = ParseDataTablesRequest(context.body);
			const auto body = ParseBody(context);
			if (!request || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");

			auto response = nlohmann::json::parse(fetcher(*request, *body), nullptr, false);
			if (response.is_discarded()) return JsonError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "Database error");
			response["draw"] = request->draw;
			JsonReply(reply, eHTTPStatusCode::OK, response);
		});
	}

	// Parse the numeric ID at the given path segment, replying 400 if it is not a valid number
	template<typename T>
	std::optional<T> RequireId(const HTTPContext& context, size_t segment, HTTPReply& reply) {
		auto id = PathId<T>(context.path, segment);
		if (!id) JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
		return id;
	}

	// Account that owns a character, for applying the account hierarchy to character actions
	std::optional<uint32_t> CharacterOwner(LWOOBJID charId, HTTPReply& reply) {
		const auto info = Database::Get()->GetCharacterInfo(charId);
		if (!info) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
			return std::nullopt;
		}
		return info->accountId;
	}

	// Checks an uploaded character XML (CharacterXmlCheck) with the CDClient, the contraband list and the owner's account
	CharacterXmlCheck::Result CheckUploadedCharacterXml(const std::string& xml, uint32_t ownerAccountId) {
		CharacterXmlCheck::Context context;
		context.ownerAccountId = ownerAccountId;
		const auto account = Database::Get()->GetAccountById(ownerAccountId);
		context.accountGmLevel = account.is_object() && account.contains("gm_level") && account["gm_level"].is_number() ? account["gm_level"].get<int32_t>() : 0;
		context.contrabandApplies = Contraband::Applies(static_cast<eGameMasterLevel>(context.accountGmLevel), ConfigFlag("contraband_ignore_staff", true));
		for (const auto& item : Database::Get()->GetContrabandItems()) context.contraband[item.lot] = { item.reason, item.action };

		auto& lookups = context.lookups;
		lookups.isItem = [](LOT lot) {
			auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT 1 FROM ComponentsRegistry WHERE id = ? AND component_type = 11 LIMIT 1;");
			stmt.bind(1, static_cast<int32_t>(lot));
			return !stmt.execQuery().eof();
		};
		lookups.stackSize = [](LOT lot) {
			auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT ic.stackSize FROM ComponentsRegistry cr JOIN ItemComponent ic ON ic.id = cr.component_id "
				"WHERE cr.id = ? AND cr.component_type = 11 LIMIT 1;");
			stmt.bind(1, static_cast<int32_t>(lot));
			auto result = stmt.execQuery();
			return result.eof() ? 0 : result.getIntField(0, 0);
		};
		lookups.missionExists = [](int32_t id) {
			auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT 1 FROM Missions WHERE id = ? LIMIT 1;");
			stmt.bind(1, id);
			return !stmt.execQuery().eof();
		};
		lookups.levelUScore = [](uint32_t level) -> std::optional<int64_t> {
			auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT requiredUScore FROM LevelProgressionLookup WHERE id = ?;");
			stmt.bind(1, static_cast<int32_t>(level));
			auto result = stmt.execQuery();
			if (result.eof()) return std::nullopt;
			return result.getInt64Field(0, 0);
		};
		auto maxLevel = CDClientDatabase::ExecuteQuery("SELECT MAX(id) FROM LevelProgressionLookup;");
		if (!maxLevel.eof()) lookups.maxLevel = static_cast<uint32_t>(maxLevel.getIntField(0, 0));
		return CharacterXmlCheck::Check(xml, context);
	}

	// Random play key in the XXXX-XXXX-XXXX-XXXX format used by NexusDashboard
	std::string GeneratePlayKey() {
		static constexpr std::string_view alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no 0/O/1/I
		std::random_device rd;
		std::uniform_int_distribution<size_t> dist(0, alphabet.size() - 1);
		std::string key;
		for (int group = 0; group < 4; group++) {
			if (group > 0) key += '-';
			for (int i = 0; i < 4; i++) key += alphabet[dist(rd)];
		}
		return key;
	}

	std::string Sessions(uint32_t count) {
		return std::to_string(count) + (count == 1 ? " online session" : " online sessions");
	}

	// Moderation history entries and kicks, shared with strike thresholds
	using AccountModeration::Note;
	using AccountModeration::KickAccount;

	std::string Reason(const nlohmann::json& body) {
		std::string reason = body.value("reason", "");
		reason.erase(0, reason.find_first_not_of(" \t"));
		reason.erase(reason.find_last_not_of(" \t") + 1);
		return reason.substr(0, 300);
	}

	// Remembered so the player can see what happened to a name they asked for, and why (optional body {reason})
	void Decide(const HTTPContext& context, const std::string& kind, int64_t subjectId, const std::string& subject, bool approved) {
		const auto body = ParseBody(context);
		Database::Get()->InsertModerationDecision(kind, subjectId, subject.substr(0, 64), approved, body ? Reason(*body).substr(0, 255) : "", static_cast<int64_t>(std::time(nullptr)));
	}

	// Apply account changes (GM level) to online sessions right away
	uint32_t RefreshAccount(uint32_t accountId, uint32_t requester) {
		PlayerActionRequest request;
		request.action = ePlayerAction::REFRESH_ACCOUNT;
		request.accountId = accountId;
		return PlayerActions::Request(request, requester, [](const PlayerActionResult& result) {
			return PlayerActions::Outcome{ true, result.affected == 0 ? "Account was not online" : "Applied to " + Sessions(result.affected) };
		});
	}

	bool ZoneExists(LWOMAPID zoneId) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT zoneID FROM ZoneTable WHERE zoneID = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(zoneId));
		auto result = stmt.execQuery();
		return !result.eof();
	}

	// Move an offline character to a zone's spawn point by editing its saved data
	bool RescueOffline(LWOOBJID charId, LWOMAPID zoneId, const std::string& spawnPoint) {
		const std::string xml = Database::Get()->GetCharacterXml(charId);
		tinyxml2::XMLDocument doc;
		if (xml.empty() || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return false;

		auto* charEl = doc.FirstChildElement("obj") ? doc.FirstChildElement("obj")->FirstChildElement("char") : nullptr;
		if (!charEl) return false;
		// Zeroed position makes the world server use the zone's default spawn point
		charEl->SetAttribute("lwid", zoneId);
		for (const char* attr : { "lzx", "lzy", "lzz", "lzrx", "lzry", "lzrz" }) charEl->SetAttribute(attr, "0");
		charEl->SetAttribute("lzrw", "1");
		// The world puts an arriving character on the spawn point its target scene names (Entity::Initialize)
		charEl->SetAttribute("tscene", spawnPoint.c_str());

		tinyxml2::XMLPrinter printer;
		doc.Print(&printer);
		Database::Get()->UpdateCharacterXml(charId, printer.CStr());
		return true;
	}

	std::optional<std::string> ReadBinaryFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) return std::nullopt;
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}

	// The PropertyTemplate row a property was claimed from, every column; path ("x y z x y z ...") becomes [[x,y,z], ...]
	nlohmann::json PropertyTemplateRow(int32_t templateId) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT * FROM PropertyTemplate WHERE id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(templateId));
		auto result = stmt.execQuery();
		if (result.eof()) return nullptr;
		nlohmann::json row = nlohmann::json::object();
		for (int i = 0; i < result.numFields(); i++) {
			const std::string column = result.fieldName(i);
			switch (result.fieldDataType(i)) {
			case SQLITE_INTEGER: row[column] = result.getInt64Field(i); break;
			case SQLITE_FLOAT: row[column] = result.getFloatField(i); break;
			case SQLITE_NULL: row[column] = nullptr; break;
			default: row[column] = result.getStringField(i); break;
			}
		}
		if (row.contains("path") && row["path"].is_string()) {
			std::istringstream stream(row["path"].get<std::string>());
			nlohmann::json points = nlohmann::json::array();
			float x, y, z;
			while (stream >> x >> y >> z) points.push_back({ x, y, z });
			row["path"] = points;
		}
		const auto prefix = "PropertyTemplate_" + std::to_string(templateId);
		row["localized_name"] = Locale::GetPhrase(prefix + "_name");
		row["localized_description"] = Locale::GetPhrase(prefix + "_description");
		const auto& zones = ZoneNames();
		for (const auto* key : { "mapID", "vendorMapID" }) {
			if (!row.contains(key) || !row[key].is_number_integer()) continue;
			const auto zone = std::to_string(row[key].get<int64_t>());
			row[std::string(key) + "_name"] = zones.contains(zone) && zones[zone].is_string() ? zones[zone].get<std::string>() : "";
		}
		return row;
	}

	// Moderators see every property; players only their own, when own_properties allows it
	std::optional<IProperty::Info> AuthorizedProperty(const HTTPContext& context, LWOOBJID propertyId, HTTPReply& reply) {
		auto info = Database::Get()->GetPropertyInfo(propertyId);
		if (!info) {
			JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
			return std::nullopt;
		}
		if (!Can(context, "properties_view")) {
			const auto owner = Database::Get()->GetCharacterInfo(info->ownerId);
			if (!owner || owner->accountId != context.accountId || !Can(context, "own_properties")) {
				JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own properties");
				return std::nullopt;
			}
		}
		return info;
	}

	std::optional<std::filesystem::path> ClientResource(const std::filesystem::path& relative) {
		const std::string clientPath = Game::config->GetValue("client_location");
		if (clientPath.empty()) return std::nullopt;
		const auto path = std::filesystem::path(clientPath) / "res" / relative;
		if (!std::filesystem::exists(path)) return std::nullopt;
		return path;
	}

	bool IsPlainFileName(std::string_view name) {
		return !name.empty() && name.size() < 128 && name.find("..") == std::string_view::npos &&
			std::ranges::all_of(name, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'; });
	}

	// LXFML for a placed property model: player-built (LOT 14) models come from the ugc table,
	// prebuilt ones from the client's res/BrickModels
	std::optional<std::pair<std::string, std::string>> ModelLxfml(const IPropertyContents::Model& model, LWOOBJID propertyId) {
		constexpr LOT UGC_MODEL_LOT = 14;
		if (model.lot == UGC_MODEL_LOT) {
			// GetUgcModels joins on properties_contents.ugc_id; GetUgcModel joins on the content id, which only
			// matches models whose content and blueprint ids happen to be equal
			for (auto& ugc : Database::Get()->GetUgcModels(propertyId)) {
				if (ugc.id != model.ugcId) continue;
				Sd0 sd0(ugc.lxfmlData);
				return std::make_pair(sd0.GetAsStringUncompressed(), "ugc_" + std::to_string(model.ugcId) + ".lxfml");
			}
			return std::nullopt;
		}

		auto stmt = CDClientDatabase::CreatePreppedStmt(
			"SELECT rc.render_asset FROM ComponentsRegistry cr JOIN RenderComponent rc ON cr.component_id = rc.id "
			"WHERE cr.component_type = 2 AND cr.id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(model.lot));
		auto result = stmt.execQuery();
		if (result.eof()) return std::nullopt;

		std::string asset = result.getStringField("render_asset", "");
		std::replace(asset.begin(), asset.end(), '\\', '/');
		asset = asset.substr(asset.rfind('/') + 1);
		asset = asset.substr(0, asset.find('.'));
		std::transform(asset.begin(), asset.end(), asset.begin(), ::tolower);
		if (!IsPlainFileName(asset)) return std::nullopt;

		const auto path = ClientResource(std::filesystem::path("BrickModels") / (asset + ".lxfml"));
		if (!path) return std::nullopt;
		const auto data = ReadBinaryFile(*path);
		if (!data) return std::nullopt;
		return std::make_pair(*data, asset + ".lxfml");
	}

	// Every LDD geometry file of a brick design (<design>.g, .g1, ...) in one bundle: uint32 count, then per part
	// uint32 length + the .g file bytes (little endian). nullopt when the design has none (or lod isn't 0-2).
	std::optional<std::string> BrickBundle(uint32_t lod, uint32_t design) {
		if (lod > 2) return std::nullopt;
		std::vector<std::string> parts;
		for (uint32_t index = 0; index < 64; index++) {
			const auto name = std::to_string(design) + ".g" + (index == 0 ? "" : std::to_string(index));
			const auto path = ClientResource(std::filesystem::path("brickprimitives") / ("lod" + std::to_string(lod)) / name);
			const auto data = path ? ReadBinaryFile(*path) : std::nullopt;
			if (!data) break;
			parts.push_back(*data);
		}
		if (parts.empty()) return std::nullopt;

		const auto writeU32 = [](std::string& out, uint32_t value) {
			for (int i = 0; i < 4; i++) out += static_cast<char>((value >> (8 * i)) & 0xff);
		};
		std::string bundle;
		writeU32(bundle, static_cast<uint32_t>(parts.size()));
		for (const auto& part : parts) {
			writeU32(bundle, static_cast<uint32_t>(part.size()));
			bundle += part;
		}
		return bundle;
	}

	bool LotExists(LOT lot) {
		auto stmt = CDClientDatabase::CreatePreppedStmt("SELECT id FROM Objects WHERE id = ? LIMIT 1;");
		stmt.bind(1, static_cast<int>(lot));
		auto result = stmt.execQuery();
		return !result.eof();
	}

	std::string ItemName(LOT lot) {
		return ClientAssets::ObjectName(lot).value_or("");
	}

	// ---- Registration by area ----

	void RegisterStatusRoutes() {
		Route(eHTTPMethod::GET, "/api/status", 0, "Server status, world instances and online player count",
			[](HTTPReply& reply, const HTTPContext& context) {
				// Which property instances run (and who could be on them) is for staff who may see online players
				auto response = Can(context, "players_view") ? ServerState::GetServerStateJson() : ServerState::PlayerSafe(ServerState::GetServerStateJson());
				response["restart"] = LiveWorld::RestartStatus();
				response["stats"]["totalAccounts"] = Database::Get()->GetAccountCount();
				response["stats"]["totalCharacters"] = Database::Get()->GetCharacterCount();
				JsonReply(reply, eHTTPStatusCode::OK, response);
			});

		Route(eHTTPMethod::GET, "/api/moderation/counts", Perm("moderate_names"), "Sizes of the moderation queues",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto snapshot = Database::Get()->GetDashboardSnapshot();
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"pendingNames", snapshot.pendingNames},
					{"pendingPetNames", snapshot.pendingPetNames},
					{"pendingProperties", snapshot.pendingProperties},
					{"unresolvedBugReports", snapshot.unresolvedBugReports},
					{"openEconomyFlags", snapshot.openEconomyFlags}
				});
			});

		Route(eHTTPMethod::GET, "/api/docs", 0, "List of API endpoints available to the caller",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json routes = nlohmann::json::array();
				for (const auto& doc : GetRouteDocs()) {
					const int16_t level = doc.permission.empty() ? doc.minGmLevel : Permissions::Level(doc.permission);
					if (level > context.gmLevel || !doc.path.starts_with("/api/") || !KeyMayUse(context, doc)) continue;
					routes.push_back({ {"method", doc.method}, {"path", doc.path}, {"minGmLevel", level}, {"permission", doc.permission}, {"description", doc.description} });
				}
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"authentication", "Send 'Authorization: Bearer <key>'. Make API keys on your account page; each has its own permissions, limits and expiry. "
						"Requests without an Authorization header, including POST /api/auth/login, must send 'X-Requested-With' (any value)."},
					{"routes", routes}
				});
			});

		Route(eHTTPMethod::GET, "/api/openapi.json", 0, "The API as an OpenAPI 3 document (what the API page shows in Swagger UI), with the endpoints available to the caller",
			[](HTTPReply& reply, const HTTPContext& context) {
				std::vector<OpenApi::Route> routes;
				for (const auto& doc : GetRouteDocs()) {
					const int16_t level = doc.permission.empty() ? doc.minGmLevel : Permissions::Level(doc.permission);
					if (level > context.gmLevel || !doc.path.starts_with("/api/") || !KeyMayUse(context, doc)) continue;
					routes.push_back({ doc.method, doc.path, doc.description, level, doc.permission });
				}
				JsonReply(reply, eHTTPStatusCode::OK, OpenApi::Build(routes, "DarkflameServer dashboard"));
			});

		// Any signed-in account, but each result is only readable by the account that started it
		Route(eHTTPMethod::GET, "/api/actions/:id", 0, "Result of an asynchronous action you started (kick, rescue, email, ...) by requestId",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto requestId = RequireId<uint32_t>(context, 2, reply);
				if (!requestId) return;
				JsonReply(reply, eHTTPStatusCode::OK, PlayerActions::GetStatus(*requestId, context.accountId));
			});

	}

	void RegisterAccountRoutes() {
		TableRoute("/api/tables/accounts", Perm("accounts_view"), "Accounts (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetAccountsTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc).dump();
		});

		Route(eHTTPMethod::GET, "/api/accounts/:id", 0, "Account details. Without accounts_view, only your own",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				if (!accountId) return;
				if (!Can(context, "accounts_view") && context.accountId != *accountId) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own account");

				auto account = Database::Get()->GetAccountById(*accountId);
				if (account.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account not found");
				account["characters"] = Database::Get()->GetAccountCharacters(*accountId);
				const auto email = Database::Get()->GetAccountEmail(*accountId);
				account["email"] = email ? email->email : "";
				account["email_confirmed"] = email && email->confirmed;
				JsonReply(reply, eHTTPStatusCode::OK, account);
			});

		Route(eHTTPMethod::POST, "/api/accounts/create", Perm("accounts_manage"), "Create an account. Body: {username, password, gm_level (optional, default 0), play_key (optional: a key string to attach; "
			"GM 0 accounts need one to log in to the game while the auth server uses keys)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string username = body->value("username", "");
				const std::string password = body->value("password", "");

				if (const auto error = ValidateUsername(username)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				if (const auto error = ValidatePassword(password)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				if (Database::Get()->GetAccountInfo(username)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Account already exists");
				const int64_t requestedLevel = body->value("gm_level", 0);
				if (requestedLevel < 0 || requestedLevel > 9 || !CanGrantGmLevel(context.gmLevel, static_cast<uint8_t>(requestedLevel))) {
					return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You cannot create an account with that GM level");
				}
				const std::string playKey = body->value("play_key", "");
				std::optional<int32_t> keyId;
				if (!playKey.empty()) {
					keyId = Database::Get()->GetRedeemablePlayKeyId(playKey);
					if (!keyId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That play key is invalid, inactive or used up");
				}

				Database::Get()->InsertNewAccount(username, HashPassword(password), static_cast<eGameMasterLevel>(requestedLevel));
				const auto created = Database::Get()->GetAccountInfo(username);
				if (created && keyId) {
					Database::Get()->SetAccountPlayKey(created->id, *keyId);
					BroadcastTableChanged("play_keys");
				}
				Audit(context, "create_account", "Created account " + username + " with GM level " + std::to_string(requestedLevel) + (keyId ? " and play key ID " + std::to_string(*keyId) : ""), AuditTarget::Account(created ? created->id : 0));
				BroadcastTableChanged("accounts");
				JsonSuccess(reply);
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/ban", Perm("accounts_ban"), "Ban or unban. Body: {banned: bool, reason (shown to the player), days (0 or missing: permanent)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto targetLevel = AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION);
				if (!targetLevel) return;

				const bool banned = body->value("banned", true);
				if (banned && RefuseLastOperator(*accountId, *targetLevel, reply, "banned")) return;
				const auto reason = Reason(*body);
				const int64_t days = std::clamp<int64_t>(body->value("days", 0), 0, 36500);
				nlohmann::json response{ {"banned", banned} };
				if (banned) {
					response["requestId"] = AccountModeration::Ban(context, *accountId, days, reason);
				} else {
					Database::Get()->SetAccountBan(*accountId, false, 0, reason);
					Note(context, *accountId, "unban", "Unbanned" + (reason.empty() ? "" : ": " + reason));
					Audit(context, "unban_account", "Account ID " + std::to_string(*accountId) + (reason.empty() ? "" : ": " + reason), AuditTarget::Account(*accountId));
					BroadcastTableChanged("accounts", std::to_string(*accountId));
				}
				JsonSuccess(reply, response);
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/lock", Perm("accounts_ban"), "Lock or unlock. Body: {locked: bool}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto targetLevel = AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION);
				if (!targetLevel) return;

				const bool locked = body->value("locked", true);
				if (locked && RefuseLastOperator(*accountId, *targetLevel, reply, "locked")) return;
				const auto reason = Reason(*body);
				Note(context, *accountId, locked ? "lock" : "unlock", std::string(locked ? "Locked" : "Unlocked") + (reason.empty() ? "" : ": " + reason));
				// accounts.locked is what the game login, the dashboard login and the account page read.
				// Unlocking also ends any failed-login throttle.
				Database::Get()->SetAccountLocked(*accountId, locked);
				if (!locked) {
					Database::Get()->SetLockout(*accountId, 0);
					DashboardAuthService::ClearThrottle(*accountId);
				}
				Audit(context, locked ? "lock_account" : "unlock_account", "Account ID " + std::to_string(*accountId), AuditTarget::Account(*accountId));
				BroadcastTableChanged("accounts", std::to_string(*accountId));

				nlohmann::json response{ {"locked", locked} };
				if (locked) response["requestId"] = KickAccount(context, *accountId, eServerDisconnectIdentifiers::KICK);
				JsonSuccess(reply, response);
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/kick", Perm("accounts_kick"), "Disconnect every online session of an account",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				if (!accountId) return;
				if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::TOOLS)) return;

				Audit(context, "kick_account", "Account ID " + std::to_string(*accountId), AuditTarget::Account(*accountId));
				JsonSuccess(reply, { {"requestId", KickAccount(context, *accountId, eServerDisconnectIdentifiers::KICK)} });
			});

		Route(eHTTPMethod::GET, "/api/accounts/:id/notes", Perm("accounts_notes"), "An account's moderation history (notes, warnings, bans, mutes, locks), newest first",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				if (!accountId) return;
				nlohmann::json notes = nlohmann::json::array();
				for (const auto& note : Database::Get()->GetAccountNotes(*accountId)) {
					notes.push_back({ {"id", note.id}, {"kind", note.kind}, {"text", note.text}, {"actor", note.actor}, {"created_at", note.createdAt} });
				}
				JsonSuccess(reply, { {"notes", notes} });
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/notes", Perm("accounts_notes"),
			"Add to an account's history. Body: {kind: note|warning, text, tell_player: bool (warnings: also show it to the player if online)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				if (Database::Get()->GetAccountById(*accountId).contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Account not found");
				const std::string kind = body->value("kind", "note");
				std::string text = body->value("text", "");
				text.erase(0, text.find_first_not_of(" \t\r\n"));
				text.erase(text.find_last_not_of(" \t\r\n") + 1);
				if (kind != "note" && kind != "warning") return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "kind must be note or warning");
				if (text.empty() || text.size() > PlayerActionRequest::MAX_TEXT) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Write up to 300 characters");
				nlohmann::json response{ {"message", kind == "warning" ? "Warning recorded" : "Note added"} };
				if (kind == "warning") {
					// A warning goes on the player's record (and maybe to them in game): only for accounts you may manage
					if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;
					if (const auto requestId = AccountModeration::Warn(context, *accountId, text, body->value("tell_player", false))) response["requestId"] = requestId;
				} else {
					Note(context, *accountId, kind, text);
					Audit(context, "note_account", "Account ID " + std::to_string(*accountId) + ": " + text, AuditTarget::Account(*accountId));
				}
				JsonSuccess(reply, response);
			});

		Route(eHTTPMethod::POST, "/api/account_notes/:id/delete", Perm("accounts_manage"), "Delete an entry from an account's history",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto id = PathId<uint64_t>(context.path, 2);
				if (!id) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid ID");
				const auto note = Database::Get()->GetAccountNote(*id);
				if (!note) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Note not found");
				if (!AuthorizeAccountAction(context, note->accountId, reply, eAccountAction::MODERATION)) return;
				Database::Get()->DeleteAccountNote(*id);
				Audit(context, "delete_account_note", "Note " + std::to_string(*id) + " (" + note->kind + "): " + note->text, AuditTarget::Account(note->accountId));
				BroadcastTableChanged("account_notes", std::to_string(note->accountId));
				JsonSuccess(reply, { {"message", "Deleted"} });
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/mute", Perm("accounts_mute"), "Mute or unmute. Body: {days: number} or {mute_until: unix time}; 0 unmutes",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;

				uint64_t muteUntil = body->value("mute_until", static_cast<uint64_t>(0));
				if (body->contains("days")) {
					const uint64_t days = std::clamp<int64_t>(body->value("days", 0), 0, 36500);
					muteUntil = days == 0 ? 0 : static_cast<uint64_t>(std::time(nullptr)) + days * 24 * 60 * 60;
				}

				AccountModeration::Mute(context, *accountId, muteUntil, Reason(*body));
				JsonSuccess(reply, { {"mute_until", muteUntil} });
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/gmlevel", Perm("accounts_gm_level"), "Set GM level. Body: {gm_level: 0-9}. Cannot grant your own level or higher unless operator; the last GM 9 that can sign in keeps GM 9",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body || !(*body)["gm_level"].is_number_integer()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "gm_level is required");

				const int64_t requested = (*body)["gm_level"].get<int64_t>();
				if (requested < 0 || requested > static_cast<int64_t>(eGameMasterLevel::OPERATOR)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid GM level");
				const auto gmLevel = static_cast<uint8_t>(requested);
				// Below GM 9 this also means nobody can ever raise their own level
				if (!CanGrantGmLevel(context.gmLevel, gmLevel)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You cannot grant a GM level equal to or above your own");
				const auto targetLevel = AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION);
				if (!targetLevel) return;
				if (gmLevel < OPERATOR_LEVEL && RefuseLastOperator(*accountId, *targetLevel, reply, "given a lower GM level")) return;

				Database::Get()->UpdateAccountGmLevel(*accountId, static_cast<eGameMasterLevel>(gmLevel));
				Audit(context, "set_gm_level", "Account ID " + std::to_string(*accountId) + " to level " + std::to_string(gmLevel), AuditTarget::Account(*accountId));
				BroadcastTableChanged("accounts", std::to_string(*accountId));
				JsonSuccess(reply, { {"gm_level", gmLevel}, {"requestId", RefreshAccount(*accountId, context.accountId)} });
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/password", Perm("accounts_manage"), "Reset another account's password. Body: {password}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!accountId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string password = body->value("password", "");
				if (const auto error = ValidatePassword(password)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);
				if (!AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION)) return;

				Database::Get()->UpdateAccountPassword(*accountId, HashPassword(password));
				// Sign the account out everywhere
				Database::Get()->SetSessionsValidAfter(*accountId, std::time(nullptr));
				Audit(context, "reset_password", "Account ID " + std::to_string(*accountId), AuditTarget::Account(*accountId));
				JsonSuccess(reply);
			});

		Route(eHTTPMethod::POST, "/api/account/password", 0, "Change your own password. Body: {current_password, new_password}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string current = body->value("current_password", "");
				const std::string next = body->value("new_password", "");
				if (const auto error = ValidatePassword(next)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, *error);

				const auto info = Database::Get()->GetAccountInfo(context.authenticatedUser);
				if (!info || current.size() > MAX_PASSWORD_LENGTH || ::bcrypt_checkpw(current.c_str(), info->bcryptPassword.c_str()) != 0) {
					return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Current password is incorrect");
				}

				Database::Get()->UpdateAccountPassword(info->id, HashPassword(next));
				// Sign out other sessions, and keep this browser signed in with a new session
				Database::Get()->SetSessionsValidAfter(info->id, std::time(nullptr));
				const auto token = JWTUtils::GenerateSessionToken(context.accountId, context.authenticatedUser, context.gmLevel, false);
				if (!token.empty()) reply.headers.push_back(AuthTokenHandler::BuildSessionCookie(token, false, UseSecureCookies()));
				Audit(context, "change_own_password", "Changed their own password on the dashboard; their other sessions were signed out");
				JsonSuccess(reply, { {"message", "Password changed. Other sessions were signed out."} });
			});

		Route(eHTTPMethod::POST, "/api/accounts/:id/delete", Perm("accounts_delete"),
			"Permanently delete an account and its characters. Body (your own account only): {confirm: your username}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto accountId = RequireId<uint32_t>(context, 2, reply);
				if (!accountId) return;
				const auto targetLevel = AuthorizeAccountAction(context, *accountId, reply, eAccountAction::MODERATION);
				if (!targetLevel || RefuseLastOperator(*accountId, *targetLevel, reply, "deleted")) return;

				// Disconnect first so an online session can't save into the deleted rows, then delete
				const std::string name = Database::Get()->GetAccountById(*accountId).value("name", std::string{});
				if (*accountId == context.accountId) {
					// Deleting your own account needs your username typed out, so it can't happen by a stray click
					const auto body = ParseBody(context).value_or(nlohmann::json::object());
					const auto& confirm = body["confirm"];
					if (!confirm.is_string() || confirm.get<std::string>() != name) {
						return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "To delete your own account, type your username (" + name + ") to confirm");
					}
				}
				PlayerActionRequest request;
				request.action = ePlayerAction::KICK_ACCOUNT;
				request.accountId = *accountId;
				request.disconnectReason = static_cast<uint32_t>(eServerDisconnectIdentifiers::FREE_TRIAL_EXPIRED);
				const auto actor = context.authenticatedUser;
				const auto actorId = context.accountId;
				const auto targetId = *accountId;
				const auto requestId = PlayerActions::Request(request, actorId, [targetId, name, actor, actorId](const PlayerActionResult& result) {
					Database::Get()->DeleteAccount(targetId);
					Database::Get()->InsertAuditLog(actorId, actor, "delete_account", "Deleted account " + name + " (ID " + std::to_string(targetId) + ")" + OwnAccountNote(actorId, targetId), targetId, 0);
					BroadcastTableChanged("accounts");
					BroadcastTableChanged("characters");
					return PlayerActions::Outcome{ true, "Account " + name + " deleted" + (result.affected ? " after disconnecting " + Sessions(result.affected) : "") };
				});
				JsonSuccess(reply, { {"requestId", requestId}, {"message", "Deleting account"} });
			});
	}

	void RegisterCharacterRoutes() {
		TableRoute("/api/tables/characters", Perm("characters_view"), "Characters (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetCharactersTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc);
		});

		TableRoute("/api/tables/pending_names", Perm("moderate_names"), "Characters with a pending name (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetPendingNamesTable(r.start, r.length).dump();
		});

		Route(eHTTPMethod::GET, "/api/characters/list", Perm("characters_view"), "Id and name of every character",
			[](HTTPReply& reply, const HTTPContext& context) {
				nlohmann::json result = nlohmann::json::array();
				for (const auto& [id, name] : Database::Get()->GetCharacterIdsAndNames()) {
					result.push_back({ {"id", std::to_string(id)}, {"name", name} });
				}
				JsonReply(reply, eHTTPStatusCode::OK, result);
			});

		Route(eHTTPMethod::GET, "/api/characters/:id", 0, "Character details. Without characters_view, only your own",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				if (!charId) return;
				const auto character = Database::Get()->GetCharacterById(*charId);
				if (character.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				if (!CanViewCharacter(context, character.value("account_id", 0u))) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
				JsonReply(reply, eHTTPStatusCode::OK, character);
			});

		Route(eHTTPMethod::GET, "/api/characters/:id/xml", 0, "Raw character XML. Without characters_view, only your own",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				if (!charId) return;
				const auto info = Database::Get()->GetCharacterInfo(*charId);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				if (!CanViewCharacter(context, info->accountId)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may only view your own characters");
				const auto xml = Database::Get()->GetCharacterXml(*charId);
				if (xml.empty()) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				reply.status = eHTTPStatusCode::OK;
				reply.message = xml;
				reply.contentType = eContentType::TEXT_PLAIN;
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/teleport", Perm("characters_rescue"),
			"Move an online character to where another online player stands (both in the same world). Body: {to: character name or ID}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!charId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto owner = CharacterOwner(*charId, reply);
				if (!owner || !AuthorizeAccountAction(context, *owner, reply, eAccountAction::TOOLS)) return;
				const auto& to = (*body)["to"];
				const auto targetId = ResolveCharacter(to.is_string() ? to.get<std::string>() : to.is_number_integer() ? to.dump() : "");
				if (!targetId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "No such character to teleport to");
				if (*targetId == *charId) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Pick another player");

				PlayerActionRequest request;
				request.action = ePlayerAction::TELEPORT_TO_PLAYER;
				request.characterId = *charId;
				request.targetId = *targetId;
				const auto actor = context.authenticatedUser;
				const auto actorId = context.accountId;
				const auto mover = *charId;
				const auto destination = *targetId;
				const auto requestId = PlayerActions::Request(request, actorId, [mover, destination, actor, actorId](const PlayerActionResult& result) {
					if (result.affected == 0) return PlayerActions::Outcome{ false, "They aren't both online in the same world. Rescue the player to that zone first." };
					const auto name = [](LWOOBJID id) { const auto info = Database::Get()->GetCharacterInfo(id); return info ? info->name : std::to_string(id); };
					const auto moved = AuditTarget::Character(mover);
					Database::Get()->InsertAuditLog(actorId, actor, "teleport_character", "Moved " + name(mover) + " to " + name(destination) + OwnAccountNote(actorId, moved.accountId), moved.accountId, moved.characterId);
					return PlayerActions::Outcome{ true, "Moved " + name(mover) + " to " + name(destination) };
				});
				JsonSuccess(reply, { {"requestId", requestId} });
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/rescue", Perm("characters_rescue"), "Move a character to a zone, live if online. Body: {zone_id, spawn_point (optional: a name from GET /api/zones/{id}/spawn_points; default the zone's own)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!charId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto owner = CharacterOwner(*charId, reply);
				if (!owner || !AuthorizeAccountAction(context, *owner, reply, eAccountAction::TOOLS)) return;

				const LWOMAPID zoneId = body->value("zone_id", 0u);
				if (zoneId == 0 || !ZoneExists(zoneId)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown zone");
				const std::string spawnPoint = body->value("spawn_point", "");
				if (!spawnPoint.empty()) {
					const auto points = ZoneSpawnPointsJson(zoneId);
					const bool known = points && std::ranges::any_of(*points, [&](const nlohmann::json& p) { return p.value("name", "") == spawnPoint; });
					if (!known) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "That zone has no spawn point called " + spawnPoint);
				}

				// If a world has the character loaded it transfers them itself; editing the saved data then
				// would be overwritten by the world's next save. Only offline characters are edited here.
				PlayerActionRequest request;
				request.action = ePlayerAction::RESCUE_CHARACTER;
				request.characterId = *charId;
				request.zoneId = zoneId;
				request.text = spawnPoint;
				const auto actor = context.authenticatedUser;
				const auto actorId = context.accountId;
				const auto target = *charId;
				const auto requestId = PlayerActions::Request(request, actorId, [target, zoneId, spawnPoint, actor, actorId](const PlayerActionResult& result) {
					const auto owner = AuditTarget::Character(target).accountId;
					const auto where = std::to_string(target) + " to zone " + std::to_string(zoneId) + (spawnPoint.empty() ? "" : " at " + spawnPoint) + OwnAccountNote(actorId, owner);
					if (result.affected > 0) {
						Database::Get()->InsertAuditLog(actorId, actor, "rescue_character", "Transferred online character " + where, owner, target);
						return PlayerActions::Outcome{ true, "Character was online and is being transferred" };
					}
					if (!RescueOffline(target, zoneId, spawnPoint)) return PlayerActions::Outcome{ false, "Failed to update the character's saved position" };
					Database::Get()->InsertAuditLog(actorId, actor, "rescue_character", "Moved offline character " + where, owner, target);
					return PlayerActions::Outcome{ true, result.timedOut
						? "Saved position updated (some world servers did not respond; if the character was online there, repeat the rescue)"
						: "Character was offline; saved position updated" };
				});
				JsonSuccess(reply, { {"requestId", requestId}, {"message", "Rescue requested"} });
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/xml", Perm("characters_edit_xml"), "Replace a character's XML (enable_char_xml_upload=1). Disconnects the owner first. "
			"Refused (400, errors) when the game couldn't load it; suspicious content (contraband, out of reach values) needs confirm=true (409, warnings). "
			"Body: {xml, confirm, remove_contraband}",
			[](HTTPReply& reply, const HTTPContext& context) {
				if (!ConfigFlag("enable_char_xml_upload", false)) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Character XML upload is disabled (enable_char_xml_upload)");
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!charId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto owner = CharacterOwner(*charId, reply);
				if (!owner || !AuthorizeAccountAction(context, *owner, reply, eAccountAction::ITEMS)) return;

				const std::string xml = body->is_object() && (*body)["xml"].is_string() ? (*body)["xml"].get<std::string>() : std::string();
				const auto check = CheckUploadedCharacterXml(xml, *owner);
				if (!check.Ok()) {
					return JsonReply(reply, eHTTPStatusCode::BAD_REQUEST, { {"success", false},
						{"error", "The XML was not stored: " + std::to_string(check.errors.size()) + (check.errors.size() >= CharacterXmlCheck::MAX_REPORTED ? "+" : "") + " problem(s) the game can't load"},
						{"errors", check.errors}, {"warnings", check.warnings} });
				}
				nlohmann::json contraband = nlohmann::json::array();
				for (const auto& found : check.contraband) {
					contraband.push_back({ {"id", std::to_string(found.item.id)}, {"lot", found.item.lot}, {"name", ClientAssets::ItemName(found.item.lot)}, {"count", found.item.count},
						{"inventory", InventoryType::InventoryTypeToString(found.item.inventory)}, {"reason", found.entry.reason},
						{"action", found.entry.action == IContraband::eContrabandAction::REMOVE ? "remove" : "flag"} });
				}
				if (!check.warnings.empty() && !body->value("confirm", false)) {
					return JsonReply(reply, eHTTPStatusCode::CONFLICT, { {"success", false}, {"confirm_required", true},
						{"error", "Check the warnings and confirm to store this XML"}, {"warnings", check.warnings}, {"contraband", contraband} });
				}

				// Contraband marked "flag and remove" only goes when the uploader chose so
				const bool removeContraband = body->value("remove_contraband", false);
				std::set<LWOOBJID> removeIds;
				if (removeContraband) for (const auto& found : check.contraband) if (found.entry.action == IContraband::eContrabandAction::REMOVE) removeIds.insert(found.item.id);
				// Store compactly like the game does
				tinyxml2::XMLDocument doc;
				doc.Parse(xml.c_str());
				tinyxml2::XMLPrinter printer(nullptr, true);
				doc.Print(&printer);
				const std::string compact = removeIds.empty() ? std::string(printer.CStr()) : CharacterXmlCheck::RemoveItems(printer.CStr(), removeIds);

				// Through the safe path: the owner is disconnected first and the current version is kept as a snapshot.
				// Once stored, what the checks found is flagged and audited like the world's own contraband check.
				const auto target = *charId;
				const auto ownerId = *owner;
				const auto actor = context.authenticatedUser;
				const auto actorId = context.accountId;
				const auto findings = check.contraband;
				const auto warnings = check.warnings;
				const auto requestId = WriteCharacterXml(*charId, *owner, context.authenticatedUser, context.accountId, "XML upload",
					[compact](const std::string&, std::string&) { return std::optional<std::string>(compact); }, nullptr,
					[target, ownerId, actor, actorId, findings, warnings, removeIds](const PlayerActions::Outcome& outcome) {
						if (!outcome.success || warnings.empty()) return;
						const auto today = static_cast<uint32_t>(std::time(nullptr) / 86400);
						for (const auto& found : findings) {
							const bool removed = removeIds.contains(found.item.id);
							Database::Get()->InsertEconomyFlag({ today, IDashboardAdmin::eFlagKind::CONTRABAND, target, found.item.lot, found.item.id, found.item.count, removed ? 1 : 0,
								std::string(removed ? "Removed from" : "Kept in") + " an uploaded character XML (" + InventoryType::InventoryTypeToString(found.item.inventory) + ", by " + actor + "): " + found.entry.reason });
						}
						std::string text = "Character " + std::to_string(target) + " XML uploaded with " + std::to_string(warnings.size()) + " warning(s)" +
							(removeIds.empty() ? "" : ", " + std::to_string(removeIds.size()) + " contraband item stack(s) removed") + ":";
						for (const auto& warning : warnings) text += "\n- " + warning;
						Database::Get()->InsertAuditLog(actorId, actor, "character_xml_warnings", text, ownerId, target);
					});
				JsonSuccess(reply, { {"requestId", requestId}, {"message", "Uploading"}, {"warnings", check.warnings}, {"removed", removeIds.size()} });
			});

		Route(eHTTPMethod::GET, "/api/characters/:id/mail", 0, "A character's mailbox (up to 100 most recent). With characters_mail, or the character's owner",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				if (!charId) return;
				const auto info = Database::Get()->GetCharacterInfo(*charId);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				if (!Can(context, "characters_mail") && info->accountId != context.accountId) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Insufficient permissions");
				nlohmann::json mail = nlohmann::json::array();
				for (const auto& m : Database::Get()->GetMailForPlayer(*charId, 100)) {
					mail.push_back({
						{"id", std::to_string(m.id)},
						{"sender", m.senderUsername},
						{"subject", m.subject},
						{"body", m.body},
						{"time_sent", m.timeSent},
						{"read", m.wasRead},
						{"attachment_lot", m.itemLOT},
						{"attachment_count", m.itemCount},
						{"attachment_name", m.itemLOT > 0 ? ItemName(m.itemLOT) : ""}
					});
				}
				JsonReply(reply, eHTTPStatusCode::OK, mail);
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/restrict", Perm("characters_restrict"), "Set trade/mail/chat restrictions. Body: {trade, mail, chat: bool}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!charId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto owner = CharacterOwner(*charId, reply);
				if (!owner || !AuthorizeAccountAction(context, *owner, reply, eAccountAction::MODERATION)) return;

				const auto info = Database::Get()->GetCharacterInfo(*charId);
				uint64_t map = static_cast<uint64_t>(info->permissionMap);
				const auto setBit = [&map, &body](const char* key, ePermissionMap bit) {
					if (!body->contains(key)) return;
					if (body->value(key, false)) map |= static_cast<uint64_t>(bit);
					else map &= ~static_cast<uint64_t>(bit);
				};
				setBit("trade", ePermissionMap::RestrictedTradeAccess);
				setBit("mail", ePermissionMap::RestrictedMailAccess);
				setBit("chat", ePermissionMap::RestrictedChatAccess);

				Database::Get()->SetCharacterPermissionMap(*charId, map);
				Audit(context, "restrict_character", "Character " + std::to_string(*charId) + " permission map " + std::to_string(map), AuditTarget::Character(*charId));
				BroadcastTableChanged("characters", std::to_string(*charId));

				PlayerActionRequest request;
				request.action = ePlayerAction::REFRESH_CHARACTER;
				request.characterId = *charId;
				const auto requestId = PlayerActions::Request(request, context.accountId, [](const PlayerActionResult& result) {
					return PlayerActions::Outcome{ true, result.affected ? "Restrictions applied to the online character" : "Restrictions saved" };
				});
				JsonSuccess(reply, { {"permission_map", map}, {"requestId", requestId} });
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/approve_name", Perm("moderate_names"), "Approve a character's pending name",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				if (!charId) return;
				const auto info = Database::Get()->GetCharacterInfo(*charId);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				if (info->pendingName.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Character has no pending name");

				const auto existing = Database::Get()->GetCharacterInfo(info->pendingName);
				if (existing && existing->id != *charId) return JsonError(reply, eHTTPStatusCode::CONFLICT, "Another character already uses this name");

				Database::Get()->SetCharacterName(*charId, info->pendingName);
				Audit(context, "approve_character_name", info->name + " -> " + info->pendingName, AuditTarget::Character(*charId));
				Decide(context, "name", *charId, info->pendingName, true);
				BroadcastTableChanged("pending_names");
				BroadcastTableChanged("characters", std::to_string(*charId));
				PlayerActionRequest request;
				request.action = ePlayerAction::NAME_MODERATED;
				request.characterId = *charId;
				request.approved = true;
				request.text = info->pendingName;
				JsonSuccess(reply, NotifyModeration(request, context.accountId));
			});

		Route(eHTTPMethod::POST, "/api/characters/:id/reject_name", Perm("moderate_names"), "Reject a character's pending name. Body (optional): {reason (shown to the player on the dashboard), strike (true: also a strike on their account)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto charId = RequireId<LWOOBJID>(context, 2, reply);
				if (!charId) return;
				const auto info = Database::Get()->GetCharacterInfo(*charId);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Character not found");
				const auto body = ParseBody(context).value_or(nlohmann::json::object());
				const auto strike = Strikes::Requested(context, body, reply);
				if (!strike) return;
				if (*strike && !AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::MODERATION)) return;

				Database::Get()->SetPendingCharacterName(*charId, "");
				Audit(context, "reject_character_name", info->name + " (rejected " + info->pendingName + ")", AuditTarget::Character(*charId));
				Decide(context, "name", *charId, info->pendingName, false);
				BroadcastTableChanged("pending_names");
				PlayerActionRequest request;
				request.action = ePlayerAction::NAME_MODERATED;
				request.characterId = *charId;
				request.approved = false;
				request.text = info->pendingName;
				auto result = NotifyModeration(request, context.accountId);
				if (*strike) Strikes::Give(context, info->accountId, *charId, eStrikeSource::NAME, info->pendingName, Reason(body)).Into(result);
				JsonSuccess(reply, result);
			});
	}

	void RegisterPlayKeyRoutes() {
		TableRoute("/api/tables/play_keys", Perm("play_keys_manage"), "Play keys with usage counts (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetPlayKeysTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc);
		});

		Route(eHTTPMethod::POST, "/api/play_keys/create", Perm("play_keys_manage"), "Create play keys. Body: {count (1-100), uses, notes, key_string (optional, single key)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const uint32_t count = std::clamp<int64_t>(body->value("count", 1), 1, MAX_PLAY_KEYS_PER_REQUEST);
				const uint32_t uses = std::clamp<int64_t>(body->value("uses", 1), 1, 100000);
				const std::string notes = body->value("notes", "");
				const std::string customKey = body->value("key_string", "");

				nlohmann::json created = nlohmann::json::array();
				if (!customKey.empty()) {
					if (customKey.size() > 64) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Key must be at most 64 characters");
					Database::Get()->CreatePlayKey(customKey, uses, notes);
					created.push_back(customKey);
				} else {
					for (uint32_t i = 0; i < count; i++) {
						const auto key = GeneratePlayKey();
						Database::Get()->CreatePlayKey(key, uses, notes);
						created.push_back(key);
					}
				}

				Audit(context, "create_play_key", std::to_string(created.size()) + " key(s) with " + std::to_string(uses) + " use(s)");
				BroadcastTableChanged("play_keys");
				JsonSuccess(reply, { {"keys", created} });
			});

		Route(eHTTPMethod::GET, "/api/play_keys/:id", Perm("play_keys_manage"), "Play key details and the accounts that used it",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto keyId = RequireId<int32_t>(context, 2, reply);
				if (!keyId) return;
				const auto key = Database::Get()->GetPlayKey(*keyId);
				if (key.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Play key not found");
				JsonReply(reply, eHTTPStatusCode::OK, key);
			});

		Route(eHTTPMethod::POST, "/api/play_keys/:id/update", Perm("play_keys_manage"), "Edit a play key. Body: {uses, notes, active}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto keyId = RequireId<int32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!keyId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const auto key = Database::Get()->GetPlayKey(*keyId);
				if (key.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Play key not found");

				const uint32_t uses = std::clamp<int64_t>(body->value("uses", key.value("key_uses", 1)), 0, 100000);
				const std::string notes = body->value("notes", key.value("notes", std::string{}));
				const bool active = body->value("active", key.value("active", true));
				Database::Get()->UpdatePlayKey(*keyId, uses, notes, active);
				Audit(context, "update_play_key", "Key ID " + std::to_string(*keyId));
				BroadcastTableChanged("play_keys", std::to_string(*keyId));
				JsonSuccess(reply);
			});

		Route(eHTTPMethod::POST, "/api/play_keys/:id/toggle", Perm("play_keys_manage"), "Activate or deactivate. Body: {active: bool}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto keyId = RequireId<int32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!keyId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const bool active = body->value("active", true);
				Database::Get()->SetPlayKeyActive(*keyId, active);
				Audit(context, active ? "activate_play_key" : "deactivate_play_key", "Key ID " + std::to_string(*keyId));
				BroadcastTableChanged("play_keys", std::to_string(*keyId));
				JsonSuccess(reply, { {"active", active} });
			});

		Route(eHTTPMethod::POST, "/api/play_keys/:id/delete", Perm("play_keys_manage"), "Delete a play key; accounts that used it are detached",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto keyId = RequireId<int32_t>(context, 2, reply);
				if (!keyId) return;
				Database::Get()->DeletePlayKey(*keyId);
				Audit(context, "delete_play_key", "Key ID " + std::to_string(*keyId));
				BroadcastTableChanged("play_keys");
				JsonSuccess(reply);
			});
	}

	void RegisterPropertyRoutes() {
		TableRoute("/api/tables/properties", Perm("properties_view"), "Properties (DataTables). Body may include {pending: true}", [](const DataTablesRequest& r, const nlohmann::json& body) {
			return WithZoneNames(Database::Get()->GetPropertiesTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc, body.value("pending", false)));
		});

		TableRoute("/api/tables/pending_properties", Perm("moderate_properties"), "Public properties awaiting review (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return WithZoneNames(Database::Get()->GetPropertiesTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc, true));
		});

		Route(eHTTPMethod::GET, "/api/properties/:id", 0, "Property details and placed models. GM 0 may only view their own",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto info = AuthorizedProperty(context, *propId, reply);
				if (!info) return;

				const auto owner = Database::Get()->GetCharacterInfo(info->ownerId);
				nlohmann::json models = nlohmann::json::array();
				for (const auto& model : Database::Get()->GetPropertyModels(*propId)) {
					models.push_back({
						{"id", std::to_string(model.id)},
						{"lot", model.lot},
						{"name", model.lot == 14 ? "Player-built model" : ItemName(model.lot)},
						{"ugc_id", std::to_string(model.ugcId)},
						{"position", {model.position.x, model.position.y, model.position.z}},
						{"rotation", {model.rotation.x, model.rotation.y, model.rotation.z, model.rotation.w}},
						{"behaviors", ModelBehaviors(model)}
					});
				}

				// Merge the rest of each model's row (player-given name, behavior ids, blueprint, creator) into its entry
				std::map<std::string, nlohmann::json> records;
				for (auto& record : Database::Get()->GetPropertyModelRecords(*propId)) {
					if (!record["id"].is_string()) continue;
					const auto id = record["id"].get<std::string>();
					records[id] = std::move(record);
				}
				for (auto& model : models) {
					const auto it = records.find(model["id"].get<std::string>());
					if (it == records.end()) continue;
					for (const auto& [key, value] : it->second.items()) {
						if (!model.contains(key)) model[key] = value;
					}
				}

				auto record = Database::Get()->GetPropertyRecord(*propId);
				const int32_t templateId = record.value("template_id", 0);
				JsonReply(reply, eHTTPStatusCode::OK, {
					{"id", std::to_string(info->id)},
					{"name", info->name},
					{"template_id", templateId},
					{"rent_amount", record.value("rent_amount", 0)},
					{"rent_due", record.value("rent_due", int64_t{ 0 })},
					{"record", record},
					{"template", PropertyTemplateRow(templateId)},
					{"description", info->description},
					{"owner_id", std::to_string(info->ownerId)},
					{"owner_name", owner ? owner->name : ""},
					{"clone_id", info->cloneId},
					{"privacy_option", info->privacyOption},
					{"mod_approved", info->modApproved != 0},
					{"rejection_reason", info->rejectionReason},
					{"reputation", info->reputation},
					{"last_updated", info->lastUpdatedTime},
					{"claimed", info->claimedTime},
					{"performance_cost", info->performanceCost},
					{"zone_id", info->zoneId},
					{"zone_name", ZoneNames().contains(std::to_string(info->zoneId)) ? ZoneNames()[std::to_string(info->zoneId)].get<std::string>() : ""},
					{"models", models}
				});
			});

		Route(eHTTPMethod::GET, "/api/properties/:id/terrain", 0, "The terrain of the zone a property is in, for the 3D viewer (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto info = AuthorizedProperty(context, *propId, reply);
				if (!info) return;
				Workers::Reply(reply, context, ZoneTerrainJsonReady(info->zoneId), [zone = info->zoneId](HTTPReply& out) {
					const auto terrain = ZoneTerrainJson(zone);
					if (!terrain) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone (is client_location set?)");
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = *terrain;
					out.headers.push_back("Cache-Control: private, max-age=86400");
				});
			});

		Route(eHTTPMethod::GET, "/api/properties/:id/terrain_chunks", 0,
			"The whole terrain of a property's zone as the game draws it: every chunk's heights, its four textures and its color and blend maps (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto info = AuthorizedProperty(context, *propId, reply);
				if (!info) return;
				Workers::Reply(reply, context, ZoneTerrainChunksReady(info->zoneId), [zone = info->zoneId](HTTPReply& out) {
					const auto terrain = ZoneTerrainChunksJson(zone);
					if (!terrain) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No terrain for this zone (is client_location set?)");
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = *terrain;
					out.headers.push_back("Cache-Control: private, max-age=86400");
				});
			});

		Route(eHTTPMethod::GET, "/api/properties/:id/scenery", 0,
			"Everything the game draws around a property: its zone's scene objects with their models and the sky, for the 3D view. Models: /api/scenery/:zone/mesh/:asset (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto info = AuthorizedProperty(context, *propId, reply);
				if (!info) return;
				Workers::Reply(reply, context, Scenery::ZoneReady(info->zoneId), [zone = info->zoneId](HTTPReply& out) {
					const auto scenery = Scenery::ZoneJson(zone);
					if (!scenery) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "No scenery for this zone (is client_location set?)");
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::APPLICATION_JSON;
					out.message = *scenery;
					out.headers.push_back("Cache-Control: private, max-age=86400");
				});
			});

		Route(eHTTPMethod::GET, "/api/terrain_textures/:id", 0, "A terrain texture (mapTextureResource ID) as PNG, for the property 3D view",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto textureId = RequireId<uint32_t>(context, 2, reply);
				if (!textureId) return;
				Workers::Reply(reply, context, TerrainTextureReady(*textureId), [id = *textureId](HTTPReply& out) {
					const auto png = TerrainTextureFile(id);
					if (!png) return JsonError(out, eHTTPStatusCode::NOT_FOUND, "Texture not found");
					out.status = eHTTPStatusCode::OK;
					out.contentType = eContentType::IMAGE_PNG;
					out.message = *png;
					out.headers.push_back("Cache-Control: private, max-age=604800");
				});
			});

		Route(eHTTPMethod::GET, "/api/properties/:id/boundary", 0, "Where a property's owner may build: the outline from the zone file and the height limit (needs client_location)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto info = AuthorizedProperty(context, *propId, reply);
				if (!info) return;
				const auto areas = ZonePropertyAreasJson(info->zoneId);
				if (!areas) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Couldn't read this zone's file (is client_location set?)");
				JsonSuccess(reply, { {"areas", *areas} });
			});

		Route(eHTTPMethod::GET, "/api/property_models/:id/lxfml", 0, "Download a placed model's LXFML (openable in LEGO Digital Designer)",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto modelId = RequireId<LWOOBJID>(context, 2, reply);
				if (!modelId) return;
				const auto model = Database::Get()->GetModel(*modelId);
				if (!model) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Model not found");

				// Authorize through the property that holds the model
				const auto owner = Database::Get()->GetModelPropertyId(*modelId);
				if (!owner || !AuthorizedProperty(context, *owner, reply)) {
					if (!owner) JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Model not found");
					return;
				}

				const auto lxfml = ModelLxfml(*model, *owner);
				if (!lxfml) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Model data not available");
				reply.status = eHTTPStatusCode::OK;
				reply.message = lxfml->first;
				reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
				reply.headers.push_back("Content-Disposition: attachment; filename=\"" + lxfml->second + "\"");
			});

		Route(eHTTPMethod::GET, "/api/bricks/:lod/:design", 0, "All LDD geometry files of a brick design, for the model viewer",
			[](HTTPReply& reply, const HTTPContext& context) {
				// Bundles <design>.g, .g1, .g2, ... so the viewer needs one request per design instead of probing for
				// each part. Format: uint32 count, then per part uint32 length + the .g file bytes (little endian).
				const auto lod = PathId<uint32_t>(context.path, 2);
				const auto design = PathId<uint32_t>(context.path, 3);
				reply.status = eHTTPStatusCode::NOT_FOUND;
				reply.message = "";
				const auto bundle = lod && design ? BrickBundle(*lod, *design) : std::nullopt;
				if (!bundle) return;
				reply.status = eHTTPStatusCode::OK;
				reply.message = *bundle;
				reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
				reply.headers.push_back("Cache-Control: private, max-age=604800");
			});

		Route(eHTTPMethod::POST, "/api/properties/:id/approve", Perm("moderate_properties"), "Approve a property",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				if (!propId) return;
				const auto property = Database::Get()->GetPropertyInfo(*propId);
				if (!property) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
				Database::Get()->ApproveProperty(*propId);
				Audit(context, "approve_property", "Property ID " + std::to_string(*propId), AuditTarget::Character(property->ownerId));
				BroadcastTableChanged("properties", std::to_string(*propId));
				PlayerActionRequest request;
				request.action = ePlayerAction::PROPERTY_MODERATED;
				request.targetId = *propId;
				request.characterId = property->ownerId;
				request.approved = true;
				JsonSuccess(reply, NotifyModeration(request, context.accountId));
			});

		Route(eHTTPMethod::POST, "/api/properties/:id/reject", Perm("moderate_properties"), "Reject a property and make it private. Body: {reason, strike (true: also a strike on the owner's account)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto propId = RequireId<LWOOBJID>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!propId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				auto info = Database::Get()->GetPropertyInfo(*propId);
				if (!info) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Property not found");
				const auto strike = Strikes::Requested(context, *body, reply);
				if (!strike) return;
				const auto owner = *strike ? Database::Get()->GetCharacterInfo(info->ownerId) : std::nullopt;
				if (owner && !AuthorizeAccountAction(context, owner->accountId, reply, eAccountAction::MODERATION)) return;

				info->modApproved = 0;
				info->rejectionReason = body->value("reason", "Rejected by moderator");
				info->privacyOption = 0;
				Database::Get()->UpdatePropertyModerationInfo(*info);
				Audit(context, "reject_property", "Property ID " + std::to_string(*propId) + " reason: " + info->rejectionReason, AuditTarget::Character(info->ownerId));
				BroadcastTableChanged("properties", std::to_string(*propId));
				PlayerActionRequest request;
				request.action = ePlayerAction::PROPERTY_MODERATED;
				request.targetId = *propId;
				request.characterId = info->ownerId;
				request.approved = false;
				request.text = info->rejectionReason;
				auto result = NotifyModeration(request, context.accountId);
				if (owner) Strikes::Give(context, owner->accountId, info->ownerId, eStrikeSource::PROPERTY, info->name, info->rejectionReason).Into(result);
				JsonSuccess(reply, result);
			});
	}

	void RegisterBugReportRoutes() {
		TableRoute("/api/tables/bug_reports", Perm("bug_reports_view"), "Bug reports (DataTables). Body may include {resolved: -1 all, 0 unresolved, 1 resolved}", [](const DataTablesRequest& r, const nlohmann::json& body) {
			const int8_t resolved = std::clamp<int64_t>(body.value("resolved", -1), -1, 1);
			return Database::Get()->GetBugReportsTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc, resolved);
		});

		Route(eHTTPMethod::GET, "/api/bug_reports/:id", Perm("bug_reports_view"), "Bug report details",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto reportId = RequireId<uint32_t>(context, 2, reply);
				if (!reportId) return;
				const auto report = Database::Get()->GetBugReport(*reportId);
				if (report.contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Bug report not found");
				JsonReply(reply, eHTTPStatusCode::OK, report);
			});

		Route(eHTTPMethod::POST, "/api/bug_reports/:id/resolve", Perm("bug_reports_manage"), "Resolve a bug report. Body: {resolution}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto reportId = RequireId<uint32_t>(context, 2, reply);
				const auto body = ParseBody(context);
				if (!reportId) return;
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string resolution = body->value("resolution", "");
				if (resolution.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "A resolution is required");
				if (Database::Get()->GetBugReport(*reportId).contains("error")) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Bug report not found");

				Database::Get()->ResolveBugReport(*reportId, context.accountId, resolution);
				Audit(context, "resolve_bug_report", "Report ID " + std::to_string(*reportId));
				BroadcastTableChanged("bug_reports", std::to_string(*reportId));
				JsonSuccess(reply);
			});

		Route(eHTTPMethod::POST, "/api/bug_reports/:id/delete", Perm("bug_reports_manage"), "Delete a bug report",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto reportId = RequireId<uint32_t>(context, 2, reply);
				if (!reportId) return;
				Database::Get()->DeleteBugReport(*reportId);
				Audit(context, "delete_bug_report", "Report ID " + std::to_string(*reportId));
				BroadcastTableChanged("bug_reports");
				JsonSuccess(reply);
			});
	}

	void RegisterPetRoutes() {
		TableRoute("/api/tables/pet_names", Perm("moderate_pet_names"), "Pet names (DataTables); rows include the pet's lot and kind. Order columns: 0 id, 2 name, 3 status, 4 owner", [](const DataTablesRequest& r, const nlohmann::json& body) {
			// The page shows the pet's kind (not sortable) as column 1; the database numbers its columns without it
			const uint32_t column = r.orderColumn >= 2 ? r.orderColumn - 1 : 0;
			return WithPetKinds(Database::Get()->GetPetNamesTable(r.start, r.length, r.search, column, r.orderAsc, body.value("pending", false)));
		});

		TableRoute("/api/tables/pending_pet_names", Perm("moderate_pet_names"), "Pet names awaiting review (DataTables); rows include the pet's lot and kind", [](const DataTablesRequest& r, const nlohmann::json&) {
			return WithPetKinds(Database::Get()->GetPetNamesTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc, true));
		});

		Route(eHTTPMethod::POST, "/api/pet_names/:id/approve", Perm("moderate_pet_names"), "Approve a pet name",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto petId = RequireId<int64_t>(context, 2, reply);
				if (!petId) return;
				const auto pet = Database::Get()->GetPetNameInfo(*petId);
				Database::Get()->ApprovePetName(*petId);
				Audit(context, "approve_pet_name", "Pet ID " + std::to_string(*petId), pet && pet->ownerId ? AuditTarget::Character(pet->ownerId) : AuditTarget{});
				Decide(context, "pet_name", *petId, pet ? pet->petName : "", true);
				BroadcastTableChanged("pet_names", std::to_string(*petId));
				PlayerActionRequest request;
				request.action = ePlayerAction::PET_NAME_MODERATED;
				request.targetId = *petId;
				request.approved = true;
				request.text = pet ? pet->petName : "";
				JsonSuccess(reply, NotifyModeration(request, context.accountId));
			});

		Route(eHTTPMethod::POST, "/api/pet_names/:id/reject", Perm("moderate_pet_names"), "Reject (delete) a pet name. Body (optional): {reason (shown to the owner on the dashboard), strike (true: also a strike on the owner's account)}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto petId = RequireId<int64_t>(context, 2, reply);
				if (!petId) return;
				const auto pet = Database::Get()->GetPetNameInfo(*petId);
				const auto body = ParseBody(context).value_or(nlohmann::json::object());
				const auto strike = Strikes::Requested(context, body, reply);
				if (!strike) return;
				const auto owner = pet && pet->ownerId ? Database::Get()->GetCharacterInfo(pet->ownerId) : std::nullopt;
				if (*strike && !owner) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "This pet's owner isn't known, so there's no account to give a strike to; nothing was done");
				if (*strike && !AuthorizeAccountAction(context, owner->accountId, reply, eAccountAction::MODERATION)) return;
				Database::Get()->RejectPetName(*petId);
				Audit(context, "reject_pet_name", "Pet ID " + std::to_string(*petId), pet && pet->ownerId ? AuditTarget::Character(pet->ownerId) : AuditTarget{});
				Decide(context, "pet_name", *petId, pet ? pet->petName : "", false);
				BroadcastTableChanged("pet_names");
				PlayerActionRequest request;
				request.action = ePlayerAction::PET_NAME_MODERATED;
				request.targetId = *petId;
				request.approved = false;
				request.text = pet ? pet->petName : "";
				auto result = NotifyModeration(request, context.accountId);
				if (*strike) Strikes::Give(context, owner->accountId, owner->id, eStrikeSource::PET_NAME, pet ? pet->petName : "", Reason(body)).Into(result);
				JsonSuccess(reply, result);
			});
	}

	void RegisterLogRoutes() {
		TableRoute("/api/tables/activity_log", Perm("logs_activity"), "Zone enter/exit activity (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetActivityLogTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc);
		});
		TableRoute("/api/tables/command_log", Perm("logs_command"), "Slash command usage (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetCommandLogTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc);
		});
		TableRoute("/api/tables/audit_log", Perm("logs_audit"), "Dashboard audit log (DataTables)", [](const DataTablesRequest& r, const nlohmann::json&) {
			return Database::Get()->GetAuditLogTable(r.start, r.length, r.search, r.orderColumn, r.orderAsc);
		});
	}

	void RegisterMailRoutes() {
		Route(eHTTPMethod::POST, "/api/mail/send", Perm("mail_send"), "Send in-game mail. Body: {recipient_id ('0' = everyone), subject, body, attachment_lot, attachment_count}",
			[](HTTPReply& reply, const HTTPContext& context) {
				const auto body = ParseBody(context);
				if (!body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Invalid JSON");
				const std::string recipient = body->value("recipient_id", "");
				const std::string subject = body->value("subject", "");
				const std::string mailBody = body->value("body", "");
				const LOT lot = body->value("attachment_lot", 0);
				const int64_t count = body->value("attachment_count", 0);

				if (subject.empty() || mailBody.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Subject and body are required");
				if (subject.size() > 50 || mailBody.size() > 400) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Subject is limited to 50 and body to 400 characters");
				const bool hasAttachment = lot > 0;
				if (hasAttachment && (count < 1 || count > MAX_MAIL_ATTACHMENT_COUNT)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Attachment count must be 1-999");
				if (hasAttachment && !LotExists(lot)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown item LOT");
				// Mailing items is as strong as /gmadditem, so it needs its own permission
				if (hasAttachment && !Can(context, "mail_items")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not attach items to mail");
				if (recipient == "0" && hasAttachment && !Can(context, "mail_broadcast_items")) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "You may not mail items to everyone");

				const auto send = [&](LWOOBJID charId, const std::string& charName) {
					MailInfo mail;
					mail.senderId = LWOOBJID_EMPTY;
					mail.senderUsername = "[GM] " + context.authenticatedUser;
					mail.receiverId = charId;
					mail.recipient = charName;
					mail.subject = subject;
					mail.body = mailBody;
					mail.timeSent = static_cast<uint64_t>(std::time(nullptr));
					mail.itemID = LWOOBJID_EMPTY;
					mail.itemSubkey = LWOOBJID_EMPTY;
					mail.itemLOT = hasAttachment ? lot : 0;
					mail.itemCount = hasAttachment ? static_cast<int16_t>(count) : 0;
					Database::Get()->InsertNewMail(mail);
				};

				const std::string attachment = hasAttachment ? " with " + std::to_string(count) + "x " + ItemName(lot) + " (" + std::to_string(lot) + ")" : "";
				if (recipient == "0") {
					auto characters = Database::Get()->GetCharacterIdsAndNames();
					// Items to everyone skip the sender's own characters unless they may give themselves items (self_items; GM 9 always)
					const bool includeOwn = !hasAttachment || (context.gmLevel >= OPERATOR_LEVEL && !context.apiKey) || Can(context, "self_items");
					if (!includeOwn) {
						const auto own = Database::Get()->GetAccountCharacterIds(context.accountId);
						std::erase_if(characters, [&](const auto& entry) { return std::find(own.begin(), own.end(), entry.first) != own.end(); });
					}
					for (const auto& [id, name] : characters) send(id, name);
					Audit(context, "broadcast_mail", "\"" + subject + "\" to " + std::to_string(characters.size()) + " characters" + attachment +
						(hasAttachment ? includeOwn ? ", their own characters included" : ", leaving out their own characters" : ""));
					return JsonSuccess(reply, { {"message", "Mail sent to " + std::to_string(characters.size()) + " characters"} });
				}

				const auto charId = GeneralUtils::TryParse<LWOOBJID>(recipient);
				const auto info = charId ? Database::Get()->GetCharacterInfo(*charId) : std::nullopt;
				if (!info) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Character not found");
				// Items only go to accounts the sender may manage; their own characters need self_items
				if (hasAttachment && !AuthorizeAccountAction(context, info->accountId, reply, eAccountAction::ITEMS)) return;
				send(info->id, info->name);
				Audit(context, "send_mail", "\"" + subject + "\" to " + info->name + attachment, AuditTarget::Character(info->id));
				JsonSuccess(reply, { {"message", "Mail sent to " + info->name} });
			});

		Route(eHTTPMethod::GET, "/api/items/search", Perm("mail_send"), "Search items by name or LOT. Query: ?q=",
			[](HTTPReply& reply, const HTTPContext& context) {
				std::string query;
				const auto pos = context.queryString.find("q=");
				if (pos != std::string::npos) {
					const auto end = context.queryString.find('&', pos);
					query = context.queryString.substr(pos + 2, end == std::string::npos ? std::string::npos : end - pos - 2);
					std::replace(query.begin(), query.end(), '+', ' ');
				}
				char decoded[256]{};
				mg_url_decode(query.c_str(), query.size(), decoded, sizeof(decoded), 1);
				JsonReply(reply, eHTTPStatusCode::OK, SearchItems(decoded));
			});
	}
}

nlohmann::json SearchItems(const std::string& query) {
	auto stmt = CDClientDatabase::CreatePreppedStmt(
		"SELECT id, name, displayName FROM Objects WHERE type = 'Loot' AND (name LIKE '%' || ? || '%' OR displayName LIKE '%' || ? || '%' OR id = ?) "
		"ORDER BY name LIMIT 50;");
	stmt.bind(1, query.c_str());
	stmt.bind(2, query.c_str());
	stmt.bind(3, GeneralUtils::TryParse<int>(query).value_or(-1));
	auto result = stmt.execQuery();

	nlohmann::json items = nlohmann::json::array();
	while (!result.eof()) {
		const std::string displayName = result.getStringField("displayName", "");
		items.push_back({ {"lot", result.getIntField("id")}, {"name", displayName.empty() ? result.getStringField("name") : displayName} });
		result.nextRow();
	}
	return items;
}

// For the property showcase (Showcase.cpp), which serves the same model data to people who aren't the owner
namespace PropertyAssets {
	std::optional<std::pair<std::string, std::string>> ModelLxfml(const IPropertyContents::Model& model, LWOOBJID propertyId) { return ::ModelLxfml(model, propertyId); }
	nlohmann::json ModelBehaviors(const IPropertyContents::Model& model) { return ::ModelBehaviors(model); }
	std::optional<std::string> BrickBundle(uint32_t lod, uint32_t design) { return ::BrickBundle(lod, design); }
}

void RegisterAPIRoutes() {
	RegisterStatusRoutes();
	RegisterAccountRoutes();
	RegisterCharacterRoutes();
	RegisterPlayKeyRoutes();
	RegisterPropertyRoutes();
	RegisterBugReportRoutes();
	RegisterPetRoutes();
	RegisterLogRoutes();
	RegisterMailRoutes();
}
