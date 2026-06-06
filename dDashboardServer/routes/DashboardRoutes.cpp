#include "DashboardRoutes.h"
#include "ServerState.h"
#include "Web.h"
#include "HTTPContext.h"
#include "eHTTPMethod.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"
#include "inja.hpp"
#include "AuthMiddleware.h"
#include "RequireAuthMiddleware.h"

void RegisterDashboardRoutes() {
	// GET / - Main dashboard page (requires authentication)
	Game::web.RegisterHTTPRoute({
		.path = "/",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Initialize inja environment
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				// Prepare data for template
				nlohmann::json data = context.GetUserDataJson();
				
				// Server status - merge with server state
				nlohmann::json serverState = ServerState::GetServerStateJson();
				data.merge_patch(serverState);
				
				// Statistics
				data["stats"]["onlinePlayers"] = 0; // TODO: Get from server communication
				data["stats"]["totalAccounts"] = Database::Get()->GetAccountCount();
				data["stats"]["totalCharacters"] = Database::Get()->GetCharacterCount();
				
				// Render template
				const std::string html = env.render_file("index.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render template\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /login - Login page (no authentication required)
	Game::web.RegisterHTTPRoute({
		.path = "/login",
		.method = eHTTPMethod::GET,
		.middleware = {},
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Initialize inja environment
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				// Render template with empty user data (not authenticated)
				nlohmann::json data = context.GetUserDataJson();
				const std::string html = env.render_file("login.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering login template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render login page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /accounts/:id - View single account
	Game::web.RegisterHTTPRoute({
		.path = "/accounts/:id",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Extract account ID from URL path
				const std::string path = context.path;
				size_t lastSlash = path.rfind('/');
				if (lastSlash == std::string::npos) {
					reply.status = eHTTPStatusCode::NOT_FOUND;
					reply.message = "<h1>404 - Account not found</h1>";
					reply.contentType = eContentType::TEXT_HTML;
					return;
				}

				std::string idStr = path.substr(lastSlash + 1);
				uint32_t accountId = 0;
				try {
					accountId = std::stoul(idStr);
				} catch (...) {
					reply.status = eHTTPStatusCode::NOT_FOUND;
					reply.message = "<h1>404 - Invalid account ID</h1>";
					reply.contentType = eContentType::TEXT_HTML;
					return;
				}

				// Permission check: GM 0 can only view own account, GM > 0 can view any account
				if (context.gmLevel == 0) {
					LOG("Regular user '%s' (GM level 0) is trying to access account ID %u", context.authenticatedUser.c_str(), accountId);
					// Regular user - get their own account ID
					auto currentUserInfo = Database::Get()->GetAccountInfo(context.authenticatedUser);
					if (!currentUserInfo.has_value() || currentUserInfo->id != accountId) {
						LOG("Permission denied: user '%s' cannot access account ID %u", context.authenticatedUser.c_str(), accountId);
						reply.status = eHTTPStatusCode::FORBIDDEN;
						reply.message = "<h1>403 - Forbidden</h1><p>You do not have permission to view this account.</p>";
						reply.contentType = eContentType::TEXT_HTML;
						return;
					}
				}

				// Get account data from API
			nlohmann::json account = Database::Get()->GetAccountById(accountId);
			
			// Check if account was found
			if (account.contains("error")) {
				reply.status = eHTTPStatusCode::NOT_FOUND;
				reply.message = "<h1>404 - Account not found</h1>";
				reply.contentType = eContentType::TEXT_HTML;
				return;
			}
				// Initialize inja environment
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);

				// Prepare data for template
				nlohmann::json data = context.GetUserDataJson();
				data["account"] = account;

				// Render template
				const std::string html = env.render_file("account-view.jinja2", data);

				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering account view template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "<h1>500 - Server Error</h1>";
				reply.contentType = eContentType::TEXT_HTML;
			}
		}
	});

	// GET /accounts - Accounts management page
	Game::web.RegisterHTTPRoute({
		.path = "/accounts",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(1) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Initialize inja environment
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);

				// Prepare data for template
				nlohmann::json data = context.GetUserDataJson();

				// Render template
				const std::string html = env.render_file("accounts.jinja2", data);

				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering accounts template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render accounts page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /characters - Characters management page
	Game::web.RegisterHTTPRoute({
		.path = "/characters",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(1) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				nlohmann::json data = context.GetUserDataJson();
				const std::string html = env.render_file("characters.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering characters template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render characters page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /play_keys - Play keys management page
	Game::web.RegisterHTTPRoute({
		.path = "/play_keys",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(1) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				nlohmann::json data = context.GetUserDataJson();
				const std::string html = env.render_file("play_keys.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering play_keys template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render play_keys page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /properties - Properties management page
	Game::web.RegisterHTTPRoute({
		.path = "/properties",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(1) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				nlohmann::json data = context.GetUserDataJson();
				const std::string html = env.render_file("properties.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering properties template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render properties page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /bug_reports - Bug reports management page
	Game::web.RegisterHTTPRoute({
		.path = "/bug_reports",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(1) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				inja::Environment env{"dDashboardServer/templates/"};
				env.set_trim_blocks(true);
				env.set_lstrip_blocks(true);
				
				nlohmann::json data = context.GetUserDataJson();
				const std::string html = env.render_file("bug_reports.jinja2", data);
				
				reply.status = eHTTPStatusCode::OK;
				reply.message = html;
				reply.contentType = eContentType::TEXT_HTML;
			} catch (const std::exception& ex) {
				LOG("Error rendering bug_reports template: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Failed to render bug_reports page\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});
}
