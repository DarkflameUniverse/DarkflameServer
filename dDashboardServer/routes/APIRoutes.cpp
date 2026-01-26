#include "APIRoutes.h"
#include "ServerState.h"
#include "Web.h"
#include "eHTTPMethod.h"
#include "json.hpp"
#include "Game.h"
#include "Database.h"
#include "Logger.h"
#include "HTTPContext.h"
#include "RequireAuthMiddleware.h"
#include <memory>

void RegisterAPIRoutes() {
	// GET /api/status - Get overall server status
	Game::web.RegisterHTTPRoute({
		.path = "/api/status",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			std::lock_guard<std::mutex> lock(ServerState::g_StatusMutex);
			
			nlohmann::json response = {
				{"auth", {
					{"online", ServerState::g_AuthStatus.online},
					{"players", ServerState::g_AuthStatus.players},
					{"version", ServerState::g_AuthStatus.version}
				}},
				{"chat", {
					{"online", ServerState::g_ChatStatus.online},
					{"players", ServerState::g_ChatStatus.players}
				}},
				{"worlds", nlohmann::json::array()}
			};
			
			for (const auto& world : ServerState::g_WorldInstances) {
				response["worlds"].push_back({
					{"mapID", world.mapID},
					{"instanceID", world.instanceID},
					{"cloneID", world.cloneID},
					{"players", world.players},
					{"isPrivate", world.isPrivate}
				});
			}
			
			reply.status = eHTTPStatusCode::OK;
			reply.message = response.dump();
			reply.contentType = eContentType::APPLICATION_JSON;
		}
	});

	// GET /api/players - Get list of online players
	Game::web.RegisterHTTPRoute({
		.path = "/api/players",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			nlohmann::json response = {
				{"players", nlohmann::json::array()},
				{"count", 0}
			};
			
			reply.status = eHTTPStatusCode::OK;
			reply.message = response.dump();
			reply.contentType = eContentType::APPLICATION_JSON;
		}
	});

	// GET /api/accounts/count - Get total account count
	Game::web.RegisterHTTPRoute({
		.path = "/api/accounts/count",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				const uint32_t count = Database::Get()->GetAccountCount();
				nlohmann::json response = {{"count", count}};
				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/accounts/count: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /api/characters/count - Get total character count
	Game::web.RegisterHTTPRoute({
		.path = "/api/characters/count",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			nlohmann::json response = {{"count", 0}, {"note", "Not yet implemented"}};
			reply.status = eHTTPStatusCode::OK;
			reply.message = response.dump();
			reply.contentType = eContentType::APPLICATION_JSON;
		}
	});
}
