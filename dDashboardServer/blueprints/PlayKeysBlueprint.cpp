#include "PlayKeysBlueprint.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "Logger.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace PlayKeysBlueprint {

	// Helper to generate a random play key string (format: XXXX-XXXX-XXXX-XXXX)
	std::string GeneratePlayKeyString() {
		static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // Excluding ambiguous chars
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

		std::stringstream ss;
		for (int i = 0; i < 16; i++) {
			if (i > 0 && i % 4 == 0) ss << '-';
			ss << charset[dis(gen)];
		}
		return ss.str();
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
		// Get all play keys (DataTables endpoint)
		CROW_ROUTE(app, "/api/playkeys")
			.methods("GET"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list data;

				try {
					auto keys = Database::Get()->GetAllPlayKeys();

					for (const auto& key : keys) {
						crow::json::wvalue item;
						item["id"] = key.id;
						item["key_string"] = key.key_string;
						item["key_uses"] = key.key_uses;
						item["times_used"] = key.times_used;
						item["active"] = key.active;
						item["notes"] = key.notes;
						item["created_at"] = static_cast<uint64_t>(key.created_at);

						data.push_back(std::move(item));
					}
				} catch (std::exception& ex) {
					// return empty list on failure
				}

				response["data"] = std::move(data);
				return crow::response(response);
			});

		// Create a new play key
		CROW_ROUTE(app, "/api/playkeys/create")
			.methods("POST"_method)
			([&](const crow::request& req) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					uint32_t count = body.has("count") ? body["count"].i() : 1;
					uint32_t uses = body.has("uses") ? body["uses"].i() : 1;
					std::string notes;
					if (body.has("notes"))
						notes = std::string(body["notes"].s());
					else
						notes = "";

					// Limit to prevent abuse
					if (count > 100) {
						response["success"] = false;
						response["error"] = "Cannot create more than 100 keys at once";
						return crow::response(response);
					}

					crow::json::wvalue::list keys;
					for (uint32_t i = 0; i < count; i++) {
						std::string keyString = GeneratePlayKeyString();
						Database::Get()->CreatePlayKey(keyString, uses, notes);
						keys.push_back(keyString);
					}

					response["success"] = true;
					response["keys"] = std::move(keys);
					response["count"] = count;

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Get single play key by ID
		CROW_ROUTE(app, "/api/playkeys/<int>")
			.methods("GET"_method)
			([&](const crow::request& req, int key_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					auto key = Database::Get()->GetPlayKeyById(key_id);
					if (!key) {
						response["success"] = false;
						response["error"] = "Play key not found";
						return crow::response(404, response);
					}

					response["success"] = true;
					response["id"] = key->id;
					response["key_string"] = key->key_string;
					response["key_uses"] = key->key_uses;
					response["times_used"] = key->times_used;
					response["active"] = key->active;
					response["notes"] = key->notes;
					response["created_at"] = static_cast<uint64_t>(key->created_at);

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Update a play key
		CROW_ROUTE(app, "/api/playkeys/<int>")
			.methods("PUT"_method, "POST"_method)
			([&](const crow::request& req, int key_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				auto body = crow::json::load(req.body);
				if (!body) {
					return crow::response(400, "{\"error\": \"Invalid JSON\"}");
				}

				crow::json::wvalue response;

				try {
					// Get current key info
					auto key = Database::Get()->GetPlayKeyById(key_id);
					if (!key) {
						response["success"] = false;
						response["error"] = "Play key not found";
						return crow::response(404, response);
					}

					uint32_t uses = body.has("uses") ? body["uses"].i() : key->key_uses;
					bool active = body.has("active") ? body["active"].b() : key->active;
					std::string notes;
					if (body.has("notes"))
						notes = std::string(body["notes"].s());
					else
						notes = key->notes;

					Database::Get()->UpdatePlayKey(key_id, uses, active, notes);

					response["success"] = true;
					response["message"] = "Play key updated successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Delete a play key
		CROW_ROUTE(app, "/api/playkeys/<int>")
			.methods("DELETE"_method)
			([&](const crow::request& req, int key_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;

				try {
					// Check if key exists
					auto key = Database::Get()->GetPlayKeyById(key_id);
					if (!key) {
						response["success"] = false;
						response["error"] = "Play key not found";
						return crow::response(404, response);
					}

					Database::Get()->DeletePlayKey(key_id);

					response["success"] = true;
					response["message"] = "Play key deleted successfully";

				} catch (std::exception& ex) {
					response["success"] = false;
					response["error"] = ex.what();
				}

				return crow::response(response);
			});

		// Get accounts associated with a play key
		CROW_ROUTE(app, "/api/playkeys/<int>/accounts")
			.methods("GET"_method)
			([&](const crow::request& req, int key_id) {
				if (!HasMinimumGMLevel(req, app, eGameMasterLevel::LEAD_MODERATOR)) {
					return crow::response(403, "{\"error\": \"Forbidden\"}");
				}

				crow::json::wvalue response;
				crow::json::wvalue::list accounts;

				try {
					// Get all accounts and filter by play_key_id
					auto allAccounts = Database::Get()->GetAllAccounts();
					for (const auto& acct : allAccounts) {
						if (acct.play_key_id == static_cast<uint32_t>(key_id)) {
							crow::json::wvalue item;
							item["id"] = acct.id;
							item["name"] = acct.name;
							item["gm_level"] = static_cast<int>(acct.gm_level);
							item["banned"] = acct.banned;
							item["locked"] = acct.locked;

							accounts.push_back(std::move(item));
						}
					}

					response["data"] = std::move(accounts);

				} catch (std::exception& ex) {
					response["error"] = ex.what();
					return crow::response(500, response);
				}

				return crow::response(response);
			});
	}

} // namespace PlayKeysBlueprint
