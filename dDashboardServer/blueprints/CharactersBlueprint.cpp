#include "CharactersBlueprint.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "ePermissionMap.h"
#include "Logger.h"

namespace CharactersBlueprint {

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

	// Helper to check if user can access a character (owns it or is GM 3+)
	bool CanAccessCharacter(const crow::request& req, DashboardApp& app, LWOOBJID characterId) {
		auto user = GetCurrentUser(req, app);
		if (!user) return false;

		// GMs can access any character
		if (static_cast<uint8_t>(user->maxGmLevel) >= static_cast<uint8_t>(eGameMasterLevel::MODERATOR)) {
			return true;
		}

		// Check if user owns this character
		auto charInfo = Database::Get()->GetCharacterInfo(characterId);
		if (charInfo && charInfo->accountId == user->id) {
			return true;
		}

		return false;
	}

	void Setup(DashboardApp& app) {
		// Get character by ID
		CROW_ROUTE(app, "/api/characters/<uint>")
			.methods("GET"_method)
			([&](const crow::request& req, uint64_t character_id) {
				if (!CanAccessCharacter(req, app, character_id)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto charInfo = Database::Get()->GetCharacterInfo(character_id);
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					response["success"] = true;
					response["id"] = static_cast<uint64_t>(charInfo->id);
					response["name"] = charInfo->name;
					response["pending_name"] = charInfo->pendingName;
					response["account_id"] = charInfo->accountId;
					response["needs_rename"] = charInfo->needsRename;
					response["clone_id"] = static_cast<uint64_t>(charInfo->cloneId);
					response["permission_map"] = static_cast<uint64_t>(charInfo->permissionMap);

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Get character XML
		CROW_ROUTE(app, "/api/characters/<uint>/xml")
			.methods("GET"_method)
			([&](const crow::request& req, uint64_t character_id) {
				if (!CanAccessCharacter(req, app, character_id)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				try {
					auto xml = Database::Get()->GetCharacterXml(character_id);

					auto res = crow::response(xml);
					res.set_header("Content-Type", "application/xml");
					res.set_header("Content-Disposition", "attachment; filename=\"character_" + std::to_string(character_id) + ".xml\"");
					return res;

				} catch (std::exception& ex) {
					crow::json::wvalue response;
					response["success"] = false;
					response["error"] = ex.what();
					return crow::response(500, response);
				}
			});

		// Rescue character (teleport to safe zone)
		CROW_ROUTE(app, "/api/characters/<uint>/rescue")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto body = crow::json::load(req.body);
					if (!body) {
						return crow::response(400, "{\"error\": \"Invalid JSON\"}");
					}

					uint32_t zoneId = 1200; // Default to Avant Gardens
					if (body.has("zone_id")) {
						zoneId = body["zone_id"].i();
					}

					// RescueCharacter logic removed; this server does not perform live rescues.
					// Return not-implemented to indicate the operation must be performed via the chat server.
					response["success"] = false;
					response["error"] = "Rescue character not implemented on this server. Use chat server tools.";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Toggle character restrictions (trade, mail, chat)
		CROW_ROUTE(app, "/api/characters/<uint>/restrict/<int>")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t character_id, int restriction_bit) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto charInfo = Database::Get()->GetCharacterInfo(character_id);
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					// Toggle the restriction bit
					uint64_t currentPerms = static_cast<uint64_t>(charInfo->permissionMap);
					uint64_t newPerms = currentPerms ^ (1ULL << restriction_bit);

					Database::Get()->UpdateCharacterPermissions(character_id, static_cast<ePermissionMap>(newPerms));

					response["success"] = true;
					response["permission_map"] = newPerms;
					response["message"] = "Character restrictions updated";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Force character rename
		CROW_ROUTE(app, "/api/characters/<uint>/force-rename")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto charInfo = Database::Get()->GetCharacterInfo(character_id);
					if (!charInfo) {
						response["success"] = false;
						response["error"] = "Character not found";
						return crow::response(404, response);
					}

					Database::Get()->SetCharacterNeedsRename(character_id, true);

					response["success"] = true;
					response["message"] = "Character will be forced to rename on next login";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Set character name (admin override)
		CROW_ROUTE(app, "/api/characters/<uint>/set-name")
			.methods("POST"_method)
			([&](const crow::request& req, uint64_t character_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::DEVELOPER)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					std::string newName = body["name"].s();

					if (newName.empty() || newName.length() > 33) {
						response["success"] = false;
						response["error"] = "Invalid name length (must be 1-33 characters)";
						return crow::response(response);
					}

					// Check if name is already in use
					if (Database::Get()->IsNameInUse(newName)) {
						response["success"] = false;
						response["error"] = "Name is already in use";
						return crow::response(response);
					}

					Database::Get()->SetCharacterName(character_id, newName);
					Database::Get()->SetPendingCharacterName(character_id, "");
					Database::Get()->SetCharacterNeedsRename(character_id, false);

					response["success"] = true;
					response["message"] = "Character name updated successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});
	}

} // namespace CharactersBlueprint
