#include "AuthRoutes.h"
#include "DashboardAuthService.h"
#include "json.hpp"
#include "Logger.h"
#include "GeneralUtils.h"
#include "Web.h"
#include "eHTTPMethod.h"
#include "HTTPContext.h"

void RegisterAuthRoutes() {
	// POST /api/auth/login
	// Request body: { "username": "string", "password": "string", "rememberMe": boolean }
	// Response: { "success": boolean, "message": "string", "token": "string", "gmLevel": number }
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/login",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				auto json = nlohmann::json::parse(context.body);
				std::string username = json.value("username", "");
				std::string password = json.value("password", "");
				bool rememberMe = json.value("rememberMe", false);

				// Validate input
				if (username.empty() || password.empty()) {
					reply.message = R"({"success":false,"message":"Username and password are required"})";
					reply.status = eHTTPStatusCode::BAD_REQUEST;
					return;
				}

				if (password.length() > 40) {
					reply.message = R"({"success":false,"message":"Password exceeds maximum length"})";
					reply.status = eHTTPStatusCode::BAD_REQUEST;
					return;
				}

				// Attempt login
				auto result = DashboardAuthService::Login(username, password, rememberMe);

				nlohmann::json response;
				response["success"] = result.success;
				response["message"] = result.message;
				if (result.success) {
					response["token"] = result.token;
					response["gmLevel"] = result.gmLevel;
				}

				reply.message = response.dump();
				reply.status = result.success ? eHTTPStatusCode::OK : eHTTPStatusCode::UNAUTHORIZED;
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const std::exception& ex) {
				LOG("Error processing login request: %s", ex.what());
				reply.message = R"({"success":false,"message":"Internal server error"})";
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/auth/verify
	// Request body: { "token": "string" }
	// Response: { "valid": boolean, "username": "string", "gmLevel": number }
	Game::web.RegisterHTTPRoute({
		.path = "/api/auth/verify",
		.method = eHTTPMethod::POST,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				auto json = nlohmann::json::parse(context.body);
				std::string token = json.value("token", "");

				if (token.empty()) {
					reply.message = R"({"valid":false})";
					reply.status = eHTTPStatusCode::BAD_REQUEST;
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				std::string username;
				uint8_t gmLevel{};
				bool valid = DashboardAuthService::VerifyToken(token, username, gmLevel);

				nlohmann::json response;
				response["valid"] = valid;
				if (valid) {
					response["username"] = username;
					response["gmLevel"] = gmLevel;
				}

				reply.message = response.dump();
				reply.status = eHTTPStatusCode::OK;
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const std::exception& ex) {
				LOG("Error processing verify request: %s", ex.what());
				reply.message = R"({"valid":false})";
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});
}
