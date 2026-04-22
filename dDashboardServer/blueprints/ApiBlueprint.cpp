#include "ApiBlueprint.h"
#include <utility>
#include "Database.h"
#include "../DashboardShared.h"
#include "eGameMasterLevel.h"
// bcrypt helpers
#include <bcrypt/BCrypt.hpp>

namespace ApiBlueprint {

	// Helper function to convert activity type to readable name
	std::string GetActivityName(eActivityType activity) {
		switch (activity) {
			case eActivityType::PlayerLoggedIn:
				return "Login";
			case eActivityType::PlayerLoggedOut:
				return "Logout";
			case eActivityType::PlayerChangedZone:
				return "Zone Change";
			default:
				return "Unknown";
		}
	}

	// Helper function to get current user's account info from session
	std::optional<IAccounts::Info> GetCurrentUser(const crow::request& req, DashboardApp& app) {
		auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
		std::string username = session.template get<std::string>("username");

		if (username.empty()) {
			return std::nullopt;
		}

		return Database::Get()->GetAccountInfo(username);
	}

	// Helper function to get user's GM level
	eGameMasterLevel GetUserGMLevel(const crow::request& req, DashboardApp& app) {
		auto user = GetCurrentUser(req, app);
		if (!user) {
			return eGameMasterLevel::CIVILIAN;
		}
		return user->maxGmLevel;
	}

	// Helper function to check if user has minimum GM level
	bool HasMinimumGMLevel(const crow::request& req, DashboardApp& app, eGameMasterLevel required) {
		auto level = GetUserGMLevel(req, app);
		return static_cast<uint8_t>(level) >= static_cast<uint8_t>(required);
	}

	void Setup(DashboardApp& app) {
		// Stats API - Server stats
		CROW_ROUTE(app, "/api/stats/server")
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				response["master_connected"] = DashboardShared::g_ConnectedToMaster.load();
				response["connected_clients"] = DashboardShared::g_ConnectedClients.load();
				response["packets_sent"] = DashboardShared::g_PacketsSent.load();
				response["packets_received"] = DashboardShared::g_PacketsReceived.load();

				return crow::response(response);
			});

		// Stats API - Account stats
		CROW_ROUTE(app, "/api/stats/accounts")
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Get total account count
					uint32_t total = Database::Get()->GetAccountCount();
					response["total"] = total;

					// Get banned and locked account counts
					uint32_t banned = Database::Get()->GetBannedAccountCount();
					uint32_t locked = Database::Get()->GetLockedAccountCount();
					response["banned"] = banned;
					response["locked"] = locked;
				} catch (std::exception& e) {
					response["total"] = 0;
					response["banned"] = 0;
					response["locked"] = 0;
				}

				return crow::response(response);
			});

		// Stats API - Character stats
		CROW_ROUTE(app, "/api/stats/characters")
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				try {
					uint64_t totalCharacters = 0;
					uint64_t pendingNames = 0;
					// Walk all accounts and sum character counts, and inspect pending names.
					for (const auto& acct : Database::Get()->GetAllAccounts()) {
						// Get characters for account
						auto chars = Database::Get()->GetAccountCharacterIds(acct.id);
						totalCharacters += chars.size();
						// Inspect pending names for each character
						for (const auto& charId : chars) {
							auto info = Database::Get()->GetCharacterInfo(charId);
							if (info && !info->pendingName.empty()) {
								pendingNames++;
							}
						}
					}
					response["total"] = static_cast<uint64_t>(totalCharacters);
					response["pending_names"] = static_cast<uint64_t>(pendingNames);
				} catch (std::exception& e) {
					response["total"] = 0;
					response["pending_names"] = 0;
				}

				return crow::response(response);
			});

		// Stats API - Recent activity
		CROW_ROUTE(app, "/api/stats/recent-activity")
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list activities;

				try {
					// Get recent activity entries from the DB
					const auto entries = Database::Get()->GetRecentActivity(50);

					for (const auto& e : entries) {
						crow::json::wvalue act;
						act["timestamp"] = e.timestamp;

						// Try to resolve character name; fall back to id string when missing
						auto charInfo = Database::Get()->GetCharacterInfo(e.characterId);
						if (charInfo) {
							act["character_name"] = charInfo->name;
						} else {
							act["character_name"] = std::to_string(static_cast<uint64_t>(e.characterId));
						}

						switch (e.activity) {
						case eActivityType::PlayerLoggedIn:
							act["activity"] = "Logged In";
							break;
						case eActivityType::PlayerLoggedOut:
							act["activity"] = "Logged Out";
							break;
						case eActivityType::PlayerChangedZone:
							act["activity"] = "Changed Zone";
							break;
						default:
							act["activity"] = "Unknown";
						}

						act["map_id"] = e.mapId;
						activities.push_back(std::move(act));
					}
				} catch (std::exception& ex) {
					// On error, return empty list
				}

				response["data"] = std::move(activities);
				return crow::response(response);
			});

		// Accounts API - List all accounts
		CROW_ROUTE(app, "/api/accounts")
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list accounts;

				try {
					for (const auto& acct : Database::Get()->GetAllAccounts()) {
						crow::json::wvalue a;
						a["id"] = acct.id;
						a["name"] = acct.name;
						a["gm_level"] = static_cast<int>(acct.gm_level);
						a["banned"] = acct.banned;
						a["locked"] = acct.locked;
						a["mute_expire"] = static_cast<uint64_t>(acct.mute_expire);
						a["play_key_id"] = acct.play_key_id;
						accounts.push_back(std::move(a));
					}
				} catch (std::exception& ex) {
					// return empty list on failure
				}

				response["data"] = std::move(accounts);
				return crow::response(response);
			});

		// Accounts API - Lock/Unlock account
		CROW_ROUTE(app, "/api/accounts/<int>/lock")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Toggle: read all accounts and find current state (simple approach)
					bool found = false;
					bool newState = false;
					for (const auto& acct : Database::Get()->GetAllAccounts()) {
						if (static_cast<int>(acct.id) == account_id) {
							found = true;
							newState = !acct.locked;
							break;
						}
					}

					if (!found) {
						response["success"] = false;
						response["error"] = "Account not found";
						return crow::response(response);
					}

					Database::Get()->UpdateAccountLock(account_id, newState);
					response["success"] = true;
					response["locked"] = newState;
				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Accounts API - Ban/Unban account
		CROW_ROUTE(app, "/api/accounts/<int>/ban")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Find current ban state
					bool found = false;
					bool newState = false;
					for (const auto& acct : Database::Get()->GetAllAccounts()) {
						if (static_cast<int>(acct.id) == account_id) {
							found = true;
							newState = !acct.banned;
							break;
						}
					}

					if (!found) {
						response["success"] = false;
						response["error"] = "Account not found";
						return crow::response(response);
					}

					Database::Get()->UpdateAccountBan(account_id, newState);
					response["success"] = true;
					response["banned"] = newState;
				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Accounts API - Mute account
		CROW_ROUTE(app, "/api/accounts/<int>/mute")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					int days = 0;
					if (body.has("days")) days = body["days"].i();

					uint64_t expire = 0;
					if (days > 0) {
						expire = static_cast<uint64_t>(time(NULL)) + static_cast<uint64_t>(days) * 24ULL * 60ULL * 60ULL;
					}

					Database::Get()->UpdateAccountUnmuteTime(account_id, expire);
					response["success"] = true;
					response["mute_expire"] = expire;
				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Activity Log API - Get activity logs with DataTables support
		CROW_ROUTE(app, "/api/activity-log")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Parse DataTables parameters (inline, removed DashboardHelpers dependency)
					struct LocalDTParams { int draw{0}; int start{0}; int length{10}; int orderColumn{-1}; std::string orderDir{"asc"}; } params;
					try {
						if (req.url_params.get("draw")) params.draw = std::stoi(req.url_params.get("draw"));
						if (req.url_params.get("start")) params.start = std::stoi(req.url_params.get("start"));
						if (req.url_params.get("length")) params.length = std::stoi(req.url_params.get("length"));
						if (req.url_params.get("order[0][column]")) params.orderColumn = std::stoi(req.url_params.get("order[0][column]"));
						if (req.url_params.get("order[0][dir]")) params.orderDir = req.url_params.get("order[0][dir]");
					} catch(...) {}

					// Map column index to column name
					const char* columns[] = { "time", "character_id", "activity", "map_id" };
					std::string orderColumn = "time";
					if (params.orderColumn >= 0 && params.orderColumn < 4) {
						orderColumn = columns[params.orderColumn];
					}

					// Get total count
					uint32_t totalRecords = Database::Get()->GetActivityLogCount();

					// Get paginated data
					auto entries = Database::Get()->GetActivityLogPaginated(
						params.start,
						params.length,
						orderColumn,
						params.orderDir
					);

					// Convert to JSON
					crow::json::wvalue::list data;
					for (const auto& entry : entries) {
						crow::json::wvalue item;
						item["character_id"] = static_cast<int64_t>(entry.characterId);
						item["activity"] = static_cast<uint32_t>(entry.activity);
						item["activity_name"] = GetActivityName(entry.activity);
						item["timestamp"] = entry.timestamp;
						item["map_id"] = static_cast<uint32_t>(entry.mapId);

						// Get character name
						auto charInfo = Database::Get()->GetCharacterInfo(entry.characterId);
						if (charInfo) {
							item["character_name"] = charInfo->name;
						} else {
							item["character_name"] = "Unknown";
						}

						data.push_back(std::move(item));
					}

					// Create DataTables response (inline)
					{
						crow::json::wvalue resp;
						resp["draw"] = params.draw;
						resp["recordsTotal"] = totalRecords;
						resp["recordsFiltered"] = totalRecords;
						resp["data"] = std::move(data);
						response = std::move(resp);
					}

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Characters API - Get characters with DataTables support
		CROW_ROUTE(app, "/api/characters")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Parse DataTables parameters (inline)
					struct LocalDTParams { int draw{0}; int start{0}; int length{10}; int orderColumn{-1}; std::string orderDir{"asc"}; } params;
					try {
						if (req.url_params.get("draw")) params.draw = std::stoi(req.url_params.get("draw"));
						if (req.url_params.get("start")) params.start = std::stoi(req.url_params.get("start"));
						if (req.url_params.get("length")) params.length = std::stoi(req.url_params.get("length"));
						if (req.url_params.get("order[0][column]")) params.orderColumn = std::stoi(req.url_params.get("order[0][column]"));
						if (req.url_params.get("order[0][dir]")) params.orderDir = req.url_params.get("order[0][dir]");
					} catch(...) {}

					// Map column index to column name
					const char* columns[] = { "id", "name", "account_id", "pending_name" };
					std::string orderColumn = "id";
					if (params.orderColumn >= 0 && params.orderColumn < 4) {
						orderColumn = columns[params.orderColumn];
					}

					// Get total count
					uint32_t totalRecords = Database::Get()->GetCharacterCount();

					// Get paginated data
					auto characters = Database::Get()->GetAllCharactersPaginated(
						params.start,
						params.length,
						orderColumn,
						params.orderDir
					);

					// Convert to JSON
					crow::json::wvalue::list data;
					for (const auto& character : characters) {
						crow::json::wvalue item;
						item["id"] = static_cast<int64_t>(character.id);
						item["name"] = character.name;
						item["pending_name"] = character.pendingName;
						item["account_id"] = character.accountId;
						item["needs_rename"] = character.needsRename;

						data.push_back(std::move(item));
					}

					// Create DataTables response (inline)
					{
						crow::json::wvalue resp;
						resp["draw"] = params.draw;
						resp["recordsTotal"] = totalRecords;
						resp["recordsFiltered"] = totalRecords;
						resp["data"] = std::move(data);
						response = std::move(resp);
					}

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Characters API - Get characters with pending names
		CROW_ROUTE(app, "/api/characters/pending-names")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto characters = Database::Get()->GetCharactersWithPendingNames();

					crow::json::wvalue::list data;
					for (const auto& character : characters) {
						crow::json::wvalue item;
						item["id"] = static_cast<int64_t>(character.id);
						item["name"] = character.name;
						item["pending_name"] = character.pendingName;
						item["account_id"] = character.accountId;

						data.push_back(std::move(item));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Characters API - Approve pending name
		CROW_ROUTE(app, "/api/characters/<int>/approve-name")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Get character info
					auto charInfo = Database::Get()->GetCharacterInfo(static_cast<LWOOBJID>(character_id));
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					// Check if there's a pending name
					if (charInfo->pendingName.empty()) {
						response["success"] = false;
						response["error"] = "No pending name to approve";
						return crow::response(response);
					}

					// Check if name is already in use
					if (Database::Get()->IsNameInUse(charInfo->pendingName)) {
						response["success"] = false;
						response["error"] = "Name is already in use";
						return crow::response(response);
					}

					// Approve the name
					Database::Get()->SetCharacterName(character_id, charInfo->pendingName);

					response["success"] = true;
					response["message"] = "Name approved successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Characters API - Reject pending name
		CROW_ROUTE(app, "/api/characters/<int>/reject-name")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Get character info
					auto charInfo = Database::Get()->GetCharacterInfo(static_cast<LWOOBJID>(character_id));
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					// Reject the name by clearing pending_name and setting needs_rename flag
					Database::Get()->SetPendingCharacterName(character_id, "");
					// Note: We'd need a new method to set needs_rename flag, for now this clears the pending name

					response["success"] = true;
					response["message"] = "Name rejected - character will need to rename";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Account API - Delete account
		CROW_ROUTE(app, "/api/accounts/<int>/delete")
			.methods("DELETE"_method, "POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::OPERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden - Requires Operator level\"}");
				}

				crow::json::wvalue response;

				try {
					// Prevent deleting own account
					auto user = GetCurrentUser(req, app);
					if (user && static_cast<int>(user->id) == account_id) {
						response["success"] = false;
						response["error"] = "Cannot delete your own account";
						return crow::response(response);
					}

					Database::Get()->DeleteAccount(account_id);

					response["success"] = true;
					response["message"] = "Account deleted successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Account API - Get account details by ID
		CROW_ROUTE(app, "/api/accounts/<int>")
			.methods("GET"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto accountInfo = Database::Get()->GetAccountById(account_id);
					if (!accountInfo) {
						response["success"] = false;
						response["error"] = "Account not found";
						return crow::response(404, response);
					}

					response["success"] = true;
					response["id"] = accountInfo->id;
					response["name"] = accountInfo->name;
					response["email"] = accountInfo->email;
					response["gm_level"] = static_cast<int>(accountInfo->gm_level);
					response["banned"] = accountInfo->banned;
					response["locked"] = accountInfo->locked;
					response["mute_expire"] = accountInfo->mute_expire;
					response["play_key_id"] = accountInfo->play_key_id;
					response["created_at"] = accountInfo->created_at;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Account API - Update GM level
		CROW_ROUTE(app, "/api/accounts/<int>/gm-level")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden - Requires Developer level\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					auto user = GetCurrentUser(req, app);
					if (!user) {
						response["success"] = false;
						response["error"] = "Not authenticated";
						return crow::response(401, response);
					}

					// Prevent editing own GM level
					if (static_cast<int>(user->id) == account_id) {
						response["success"] = false;
						response["error"] = "Cannot edit your own GM level";
						return crow::response(response);
					}

					int newLevel = body["gm_level"].i();

					// Prevent level 8+ from editing each other (only level 9 can edit level 8)
					auto targetAccount = Database::Get()->GetAccountById(account_id);
					if (targetAccount) {
						uint8_t targetGMLevel = static_cast<uint8_t>(targetAccount->gm_level);
						uint8_t userGMLevel = static_cast<uint8_t>(user->maxGmLevel);

						if (targetGMLevel >= 8 && userGMLevel < 9) {
							response["success"] = false;
							response["error"] = "Cannot edit GM level 8+ accounts";
							return crow::response(response);
						}
					}

					Database::Get()->UpdateAccountGmLevel(account_id, static_cast<eGameMasterLevel>(newLevel));

					response["success"] = true;
					response["message"] = "GM level updated successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

			// Registration API - create account using a play key
			CROW_ROUTE(app, "/api/register")
				.methods("POST"_method)
				([&](const crow::request& req) {
					crow::json::wvalue response;
					auto body = crow::json::load(req.body);
					if (!body) {
						response["success"] = false;
						response["error"] = "Invalid JSON";
						return crow::response(400, response);
					}

					std::string username;
					std::string password;
					std::string playkey;

					if (body.has("username")) username = std::string(body["username"].s());
					if (body.has("password")) password = std::string(body["password"].s());
					if (body.has("play_key")) playkey = std::string(body["play_key"].s());

					if (username.empty() || password.empty() || playkey.empty()) {
						response["success"] = false;
						response["error"] = "username, password and play_key are required";
						return crow::response(400, response);
					}

					try {
						// Ensure username isn't already taken
						if (Database::Get()->IsNameInUse(username)) {
							response["success"] = false;
							response["error"] = "Username already in use";
							return crow::response(409, response);
						}

						// Lookup playkey by string
						auto keyInfo = Database::Get()->GetPlayKeyByString(playkey);
						if (!keyInfo) {
							response["success"] = false;
							response["error"] = "Play key not found";
							return crow::response(404, response);
						}

						// Check active and usage
						if (!keyInfo->active) {
							response["success"] = false;
							response["error"] = "Play key is inactive";
							return crow::response(400, response);
						}

						if (static_cast<int>(keyInfo->times_used) >= static_cast<int>(keyInfo->key_uses)) {
							response["success"] = false;
							response["error"] = "Play key has no remaining uses";
							return crow::response(400, response);
						}

						// Hash password with bcrypt
						std::string hash = BCrypt::generateHash(password);

						// Insert account
						Database::Get()->InsertNewAccount(username, hash);

						// Retrieve account id
						auto acctInfo = Database::Get()->GetAccountInfo(username);
						if (!acctInfo) {
							response["success"] = false;
							response["error"] = "Failed to create account";
							return crow::response(500, response);
						}

						// Link playkey to account
						Database::Get()->UpdateAccountPlayKey(acctInfo->id, keyInfo->id);

						// Consume one usage of playkey
						Database::Get()->ConsumePlayKeyUsage(keyInfo->id);

						response["success"] = true;
						response["account_id"] = acctInfo->id;
						return crow::response(response);
					} catch (std::exception& ex) {
						response["success"] = false;
						response["error"] = ex.what();
						return crow::response(500, response);
					}
				});

		// Account API - Update email
		CROW_ROUTE(app, "/api/accounts/<int>/email")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden - Requires Developer level\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					std::string newEmail = body["email"].s();

					if (newEmail.empty() || newEmail.length() > 255) {
						response["success"] = false;
						response["error"] = "Invalid email";
						return crow::response(response);
					}

					Database::Get()->UpdateAccountEmail(account_id, newEmail);

					response["success"] = true;
					response["message"] = "Email updated successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Account API - Reset password
		CROW_ROUTE(app, "/api/accounts/<int>/password-reset")
			.methods("POST"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::OPERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden - Requires Operator level\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					std::string newPassword = body["password"].s();

					if (newPassword.empty() || newPassword.length() < 8) {
						response["success"] = false;
						response["error"] = "Password must be at least 8 characters";
						return crow::response(response);
					}

					// Hash the password with bcrypt
					std::string hashedPassword = BCrypt::generateHash(newPassword);

					Database::Get()->UpdateAccountPassword(account_id, hashedPassword);

					response["success"] = true;
					response["message"] = "Password reset successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Get character details by ID
		CROW_ROUTE(app, "/api/characters/<int>")
			.methods("GET"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto charInfo = Database::Get()->GetCharacterInfo(static_cast<LWOOBJID>(character_id));
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					response["success"] = true;
					response["id"] = static_cast<int64_t>(charInfo->id);
					response["name"] = charInfo->name;
					response["pending_name"] = charInfo->pendingName;
					response["needs_rename"] = charInfo->needsRename;
					response["account_id"] = static_cast<int64_t>(charInfo->accountId);
					response["level"] = charInfo->level;
					response["uscore"] = static_cast<int64_t>(charInfo->uscore);
					response["zone_id"] = charInfo->zoneId;
					response["last_login"] = static_cast<uint64_t>(charInfo->lastLogin);
					response["created_on"] = static_cast<uint64_t>(charInfo->createdOn);

					// Get account name
					auto accountInfo = Database::Get()->GetAccountById(charInfo->accountId);
					if (accountInfo) {
						response["account_name"] = accountInfo->name;
					}

					// TODO: Add restriction flags when implemented in database
					response["mail_restricted"] = false;
					response["trade_restricted"] = false;
					response["chat_restricted"] = false;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Get character stats
		CROW_ROUTE(app, "/api/characters/<int>/stats")
			.methods("GET"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto stats = Database::Get()->GetCharacterStats(static_cast<LWOOBJID>(character_id));
					if (!stats) {
						response["success"] = false;
						response["error"] = "Character stats not found";
						return crow::response(404, response);
					}

					response["success"] = true;
					response["total_currency_collected"] = static_cast<uint64_t>(stats->totalCurrencyCollected);
					response["total_bricks_collected"] = static_cast<uint64_t>(stats->totalBricksCollected);
					response["total_smashables"] = static_cast<uint64_t>(stats->totalSmashables);
					response["total_quickbuilds_completed"] = static_cast<uint64_t>(stats->totalQuickbuildsCompleted);
					response["total_enemies_smashed"] = static_cast<uint64_t>(stats->totalEnemiesSmashed);
					response["total_rockets_used"] = static_cast<uint64_t>(stats->totalRocketsUsed);
					response["total_missions_completed"] = static_cast<uint64_t>(stats->totalMissionsCompleted);
					response["total_pets_tamed"] = static_cast<uint64_t>(stats->totalPetsTamed);

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Get character inventory
		CROW_ROUTE(app, "/api/characters/<int>/inventory")
			.methods("GET"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto items = Database::Get()->GetCharacterInventory(static_cast<LWOOBJID>(character_id));

					crow::json::wvalue::list data;
					for (const auto& item : items) {
						crow::json::wvalue i;
						i["item_id"] = static_cast<int64_t>(item.itemId);
						i["count"] = item.count;
						i["slot"] = item.slot;
						data.push_back(std::move(i));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Character API - Get character activity
		CROW_ROUTE(app, "/api/characters/<int>/activity")
			.methods("GET"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto activities = Database::Get()->GetCharacterActivity(static_cast<LWOOBJID>(character_id), 50);

					crow::json::wvalue::list data;
					for (const auto& activity : activities) {
						crow::json::wvalue a;
						a["timestamp"] = activity.timestamp;
						a["activity"] = GetActivityName(activity.activity);
						a["map_id"] = activity.mapId;
						data.push_back(std::move(a));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Character API - Rescue character
		CROW_ROUTE(app, "/api/characters/<int>/rescue")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Rescue moves character to a safe spawn location (Venture Explorer)
					const uint32_t VENTURE_EXPLORER_ZONE = 1200;
					Database::Get()->RescueCharacter(static_cast<LWOOBJID>(character_id), VENTURE_EXPLORER_ZONE);

					response["success"] = true;
					response["message"] = "Character rescued to Venture Explorer";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Delete character
		CROW_ROUTE(app, "/api/characters/<int>/delete")
			.methods("POST"_method, "DELETE"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden - Requires Developer level\"}");
				}

				crow::json::wvalue response;

				try {
					Database::Get()->DeleteCharacter(static_cast<LWOOBJID>(character_id));

					response["success"] = true;
					response["message"] = "Character deleted successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Toggle mail restriction
		CROW_ROUTE(app, "/api/characters/<int>/toggle-mail")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// TODO: Implement when database supports restriction flags
					response["success"] = true;
					response["message"] = "Mail restriction toggled (not yet implemented)";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Toggle trade restriction
		CROW_ROUTE(app, "/api/characters/<int>/toggle-trade")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// TODO: Implement when database supports restriction flags
					response["success"] = true;
					response["message"] = "Trade restriction toggled (not yet implemented)";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Character API - Toggle chat restriction
		CROW_ROUTE(app, "/api/characters/<int>/toggle-chat")
			.methods("POST"_method)
			([&](const crow::request& req, int character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// TODO: Implement when database supports restriction flags
					response["success"] = true;
					response["message"] = "Chat restriction toggled (not yet implemented)";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// User API - Get current user's characters
		CROW_ROUTE(app, "/api/user/characters")
			.methods("GET"_method)
			([&](const crow::request& req) {
				auto user = GetCurrentUser(req, app);
				if (!user) {
					return crow::response(401, "{\"error\": \"Not authenticated\"}");
				}

				crow::json::wvalue response;

				try {
					auto charIds = Database::Get()->GetAccountCharacterIds(user->id);

					crow::json::wvalue::list data;
					for (const auto& charId : charIds) {
						auto charInfo = Database::Get()->GetCharacterInfo(charId);
						if (charInfo) {
							crow::json::wvalue c;
							c["id"] = static_cast<int64_t>(charInfo->id);
							c["name"] = charInfo->name;
							c["pending_name"] = charInfo->pendingName;
							c["needs_rename"] = charInfo->needsRename;
							c["level"] = charInfo->level;
							c["uscore"] = static_cast<int64_t>(charInfo->uscore);
							c["zone_id"] = charInfo->zoneId;
							c["last_login"] = static_cast<uint64_t>(charInfo->lastLogin);
							data.push_back(std::move(c));
						}
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Account API - Get account's characters
		CROW_ROUTE(app, "/api/accounts/<int>/characters")
			.methods("GET"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto charIds = Database::Get()->GetAccountCharacterIds(account_id);

					crow::json::wvalue::list data;
					for (const auto& charId : charIds) {
						auto charInfo = Database::Get()->GetCharacterInfo(charId);
						if (charInfo) {
							crow::json::wvalue c;
							c["id"] = static_cast<int64_t>(charInfo->id);
							c["name"] = charInfo->name;
							c["level"] = charInfo->level;
							c["map_id"] = charInfo->zoneId;
							c["last_login"] = static_cast<uint64_t>(charInfo->lastLogin);
							data.push_back(std::move(c));
						}
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Account API - Get account's sessions
		CROW_ROUTE(app, "/api/accounts/<int>/sessions")
			.methods("GET"_method)
			([&](const crow::request& req, int account_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto sessions = Database::Get()->GetAccountSessions(account_id);

					crow::json::wvalue::list data;
					for (const auto& session : sessions) {
						crow::json::wvalue s;
						s["session_id"] = static_cast<int64_t>(session.sessionId);
						s["ip_address"] = session.ipAddress;
						s["login_time"] = static_cast<uint64_t>(session.loginTime);
						s["logout_time"] = static_cast<uint64_t>(session.logoutTime);
						s["active"] = session.active;
						data.push_back(std::move(s));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Logs API - Get command logs
		CROW_ROUTE(app, "/api/logs/commands")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto logs = Database::Get()->GetCommandLogs(100);

					crow::json::wvalue::list data;
					for (const auto& log : logs) {
						crow::json::wvalue l;
						l["timestamp"] = static_cast<uint64_t>(log.timestamp);
						l["character_id"] = static_cast<int64_t>(log.characterId);
						l["command"] = log.command;
						l["arguments"] = log.arguments;

						// Get character name
						auto charInfo = Database::Get()->GetCharacterInfo(log.characterId);
						if (charInfo) {
							l["character_name"] = charInfo->name;
						} else {
							l["character_name"] = "Unknown";
						}

						data.push_back(std::move(l));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});

		// Logs API - Get audit logs
		CROW_ROUTE(app, "/api/logs/audits")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto logs = Database::Get()->GetAuditLogs(100);

					crow::json::wvalue::list data;
					for (const auto& log : logs) {
						crow::json::wvalue l;
						l["timestamp"] = static_cast<uint64_t>(log.timestamp);
						l["admin_account_id"] = static_cast<int64_t>(log.adminAccountId);
						l["action"] = log.action;
						l["target_type"] = log.targetType;
						l["target_id"] = static_cast<int64_t>(log.targetId);
						l["details"] = log.details;

						// Get admin username
						auto adminInfo = Database::Get()->GetAccountById(log.adminAccountId);
						if (adminInfo) {
							l["admin_username"] = adminInfo->name;
						} else {
							l["admin_username"] = "Unknown";
						}

						data.push_back(std::move(l));
					}

					response["data"] = std::move(data);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});
	}

} // namespace ApiBlueprint
