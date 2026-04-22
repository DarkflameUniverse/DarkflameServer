#include "AuthBlueprint.h"
#include "Database.h"
#include <bcrypt/BCrypt.hpp>

namespace AuthBlueprint {

	void Setup(DashboardApp& app) {
		// Login route
		CROW_ROUTE(app, "/api/login")
			.methods("POST"_method)
			([&](crow::request& req, crow::response& res) {
				auto body = crow::json::load(req.body);
				if (!body) {
					res.code = 400;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Invalid JSON\"}");
					res.end();
					return;
				}

				std::string username = body["username"].s();
				std::string password = body["password"].s();

				if (username.empty() || password.empty()) {
					res.code = 400;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Username and password required\"}");
					res.end();
					return;
				}

				// Get account info from database
				auto accountInfo = Database::Get()->GetAccountInfo(username);
				if (!accountInfo) {
					res.code = 401;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Invalid credentials\"}");
					res.end();
					return;
				}

				// Verify password using bcrypt
				if (!BCrypt::validatePassword(password, accountInfo->bcryptPassword)) {
					res.code = 401;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Invalid credentials\"}");
					res.end();
					return;
				}

				// Check if account is banned or locked
				if (accountInfo->banned) {
					res.code = 403;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Account is banned\"}");
					res.end();
					return;
				}

				if (accountInfo->locked) {
					res.code = 403;
					res.set_header("Content-Type", "application/json");
					res.write("{\"error\": \"Account is locked\"}");
					res.end();
					return;
				}

				// Create session
				auto& session = app.get_context<Session>(req);
				session.set("username", username);
				session.set("account_id", static_cast<int>(accountInfo->id));
				session.set("gm_level", static_cast<int>(accountInfo->maxGmLevel));

				// Return success with user info
				crow::json::wvalue response;
				response["success"] = true;
				response["username"] = username;
				response["account_id"] = accountInfo->id;
				response["gm_level"] = static_cast<uint8_t>(accountInfo->maxGmLevel);

				res.set_header("Content-Type", "application/json");
				res.write(response.dump());
				res.end();
			});

		// Logout route
		CROW_ROUTE(app, "/api/logout")
			.methods("POST"_method)
			([&](crow::request& req, crow::response& res) {
				auto& session = app.get_context<Session>(req);

				// Clear session
				session.remove("username");
				session.remove("account_id");
				session.remove("gm_level");

				crow::json::wvalue response;
				response["success"] = true;

				res.set_header("Content-Type", "application/json");
				res.write(response.dump());
				res.end();
			});

		// Auth status route
		CROW_ROUTE(app, "/api/auth/status")
			([&](const crow::request& req) {
				auto& session = app.get_context<Session>(const_cast<crow::request&>(req));
				std::string username = session.template get<std::string>("username");

				crow::json::wvalue response;

				if (!username.empty()) {
					int account_id = session.template get<int>("account_id", -1);
					int gm_level = session.template get<int>("gm_level", -1);

					response["authenticated"] = true;
					response["username"] = username;
					response["account_id"] = account_id;
					response["gm_level"] = gm_level;
				} else {
					response["authenticated"] = false;
				}

				return crow::response(response);
			});
	}

} // namespace AuthBlueprint
