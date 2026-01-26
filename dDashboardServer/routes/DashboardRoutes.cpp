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
				nlohmann::json data;
				// Get username from auth context
				data["username"] = context.authenticatedUser;
				data["gmLevel"] = context.gmLevel;
				
				// Server status (placeholder data - will be updated with real data from master)
				data["auth"]["online"] = ServerState::g_AuthStatus.online;
				data["auth"]["players"] = ServerState::g_AuthStatus.players;
				data["chat"]["online"] = ServerState::g_ChatStatus.online;
				data["chat"]["players"] = ServerState::g_ChatStatus.players;
				
				// World instances
				std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
				data["worlds"] = nlohmann::json::array();
				for (const auto& world : ServerState::g_WorldInstances) {
					data["worlds"].push_back({
						{"mapID", world.mapID},
						{"instanceID", world.instanceID},
						{"cloneID", world.cloneID},
						{"players", world.players},
						{"isPrivate", world.isPrivate}
					});
				}
				
				// Statistics
				const uint32_t accountCount = Database::Get()->GetAccountCount();
				data["stats"]["onlinePlayers"] = 0; // TODO: Get from server communication
				data["stats"]["totalAccounts"] = accountCount;
				data["stats"]["totalCharacters"] = 0; // TODO: Add GetCharacterCount to database interface
				
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
				
				// Render template with empty username
				nlohmann::json data;
				data["username"] = "";
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
}
