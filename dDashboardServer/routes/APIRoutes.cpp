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
			nlohmann::json response = ServerState::GetServerStateJson();
			
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
			try {
				const uint32_t count = Database::Get()->GetCharacterCount();
				nlohmann::json response = {{"count", count}};
				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/characters/count: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/tables/accounts - Get accounts table data (DataTables.js format)
	Game::web.RegisterHTTPRoute({
		.path = "/api/tables/accounts",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Only admins (GM > 0) can access table data
				if (context.gmLevel == 0) {
					reply.status = eHTTPStatusCode::FORBIDDEN;
					reply.message = "{\"error\":\"Forbidden - Admin access required\"}"; 
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				nlohmann::json requestData = nlohmann::json::parse(context.body);

				// Extract DataTables parameters
				uint32_t draw = requestData.value("draw", 1);
				uint32_t start = requestData.value("start", 0);
				uint32_t length = requestData.value("length", 10);
				
				// Extract search - it can be a string or an object with a "value" property
				std::string search = "";
				if (requestData.contains("search")) {
					if (requestData["search"].is_string()) {
						search = requestData["search"].get<std::string>();
					} else if (requestData["search"].is_object() && requestData["search"].contains("value")) {
						search = requestData["search"]["value"].get<std::string>();
					}
				}
				
				uint32_t orderColumn = 0;
				bool orderAsc = true;

				// Extract order parameters
				if (requestData.contains("order") && requestData["order"].is_array() && requestData["order"].size() > 0) {
					orderColumn = requestData["order"][0].value("column", 0);
					orderAsc = requestData["order"][0].value("dir", "asc") == "asc";
				}

				// Get the accounts table data
			nlohmann::json response = Database::Get()->GetAccountsTable(start, length, search, orderColumn, orderAsc);

				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/tables/accounts: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/tables/accounts: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/tables/characters - Get characters table data (DataTables.js format)
	Game::web.RegisterHTTPRoute({
		.path = "/api/tables/characters",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Only admins (GM > 0) can access table data
				if (context.gmLevel == 0) {
					reply.status = eHTTPStatusCode::FORBIDDEN;
					reply.message = "{\"error\":\"Forbidden - Admin access required\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				nlohmann::json requestData = nlohmann::json::parse(context.body);

				uint32_t draw = requestData.value("draw", 1);
				uint32_t start = requestData.value("start", 0);
				uint32_t length = requestData.value("length", 10);
				
				std::string search = "";
				if (requestData.contains("search")) {
					if (requestData["search"].is_string()) {
						search = requestData["search"].get<std::string>();
					} else if (requestData["search"].is_object() && requestData["search"].contains("value")) {
						search = requestData["search"]["value"].get<std::string>();
					}
				}
				
				uint32_t orderColumn = 0;
				bool orderAsc = true;

				if (requestData.contains("order") && requestData["order"].is_array() && requestData["order"].size() > 0) {
					orderColumn = requestData["order"][0].value("column", 0);
					orderAsc = requestData["order"][0].value("dir", "asc") == "asc";
				}

				std::string tableData = Database::Get()->GetCharactersTable(start, length, search, orderColumn, orderAsc);

				nlohmann::json response = nlohmann::json::parse(tableData);
				response["draw"] = draw;

				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/tables/characters: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/tables/characters: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/tables/play_keys - Get play keys table data (DataTables.js format)
	Game::web.RegisterHTTPRoute({
		.path = "/api/tables/play_keys",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {				// Only admins (GM > 0) can access table data
				if (context.gmLevel == 0) {
					reply.status = eHTTPStatusCode::FORBIDDEN;
					reply.message = "{\"error\":\"Forbidden - Admin access required\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}
				nlohmann::json requestData = nlohmann::json::parse(context.body);

				uint32_t draw = requestData.value("draw", 1);
				uint32_t start = requestData.value("start", 0);
				uint32_t length = requestData.value("length", 10);
				
				std::string search = "";
				if (requestData.contains("search")) {
					if (requestData["search"].is_string()) {
						search = requestData["search"].get<std::string>();
					} else if (requestData["search"].is_object() && requestData["search"].contains("value")) {
						search = requestData["search"]["value"].get<std::string>();
					}
				}
				
				uint32_t orderColumn = 0;
				bool orderAsc = true;

				if (requestData.contains("order") && requestData["order"].is_array() && requestData["order"].size() > 0) {
					orderColumn = requestData["order"][0].value("column", 0);
					orderAsc = requestData["order"][0].value("dir", "asc") == "asc";
				}

				std::string tableData = Database::Get()->GetPlayKeysTable(start, length, search, orderColumn, orderAsc);

				nlohmann::json response = nlohmann::json::parse(tableData);
				response["draw"] = draw;

				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/tables/play_keys: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/tables/play_keys: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/tables/properties - Get properties table data (DataTables.js format)
	Game::web.RegisterHTTPRoute({
		.path = "/api/tables/properties",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Only admins (GM > 0) can access table data
				if (context.gmLevel == 0) {
					reply.status = eHTTPStatusCode::FORBIDDEN;
					reply.message = "{\"error\":\"Forbidden - Admin access required\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				nlohmann::json requestData = nlohmann::json::parse(context.body);

				uint32_t draw = requestData.value("draw", 1);
				uint32_t start = requestData.value("start", 0);
				uint32_t length = requestData.value("length", 10);
				
				std::string search = "";
				if (requestData.contains("search")) {
					if (requestData["search"].is_string()) {
						search = requestData["search"].get<std::string>();
					} else if (requestData["search"].is_object() && requestData["search"].contains("value")) {
						search = requestData["search"]["value"].get<std::string>();
					}
				}
				
				uint32_t orderColumn = 0;
				bool orderAsc = true;

				if (requestData.contains("order") && requestData["order"].is_array() && requestData["order"].size() > 0) {
					orderColumn = requestData["order"][0].value("column", 0);
					orderAsc = requestData["order"][0].value("dir", "asc") == "asc";
				}

				std::string tableData = Database::Get()->GetPropertiesTable(start, length, search, orderColumn, orderAsc);

				nlohmann::json response = nlohmann::json::parse(tableData);
				response["draw"] = draw;

				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/tables/properties: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/tables/properties: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// POST /api/tables/bug_reports - Get bug reports table data (DataTables.js format)
	Game::web.RegisterHTTPRoute({
		.path = "/api/tables/bug_reports",
		.method = eHTTPMethod::POST,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {				// Only admins (GM > 0) can access table data
				if (context.gmLevel == 0) {
					reply.status = eHTTPStatusCode::FORBIDDEN;
					reply.message = "{\"error\":\"Forbidden - Admin access required\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}
				nlohmann::json requestData = nlohmann::json::parse(context.body);

				uint32_t draw = requestData.value("draw", 1);
				uint32_t start = requestData.value("start", 0);
				uint32_t length = requestData.value("length", 10);
				
				std::string search = "";
				if (requestData.contains("search")) {
					if (requestData["search"].is_string()) {
						search = requestData["search"].get<std::string>();
					} else if (requestData["search"].is_object() && requestData["search"].contains("value")) {
						search = requestData["search"]["value"].get<std::string>();
					}
				}
				
				uint32_t orderColumn = 0;
				bool orderAsc = true;

				if (requestData.contains("order") && requestData["order"].is_array() && requestData["order"].size() > 0) {
					orderColumn = requestData["order"][0].value("column", 0);
					orderAsc = requestData["order"][0].value("dir", "asc") == "asc";
				}

				std::string tableData = Database::Get()->GetBugReportsTable(start, length, search, orderColumn, orderAsc);

				nlohmann::json response = nlohmann::json::parse(tableData);
				response["draw"] = draw;

				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/tables/bug_reports: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/tables/bug_reports: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});

	// GET /api/accounts/:id - Get single account by ID
	Game::web.RegisterHTTPRoute({
		.path = "/api/accounts/:id",
		.method = eHTTPMethod::GET,
		.middleware = { std::make_shared<RequireAuthMiddleware>(0) },
		.handle = [](HTTPReply& reply, const HTTPContext& context) {
			try {
				// Extract account ID from URL path
				const std::string path = context.path;
				size_t lastSlash = path.rfind('/');
				if (lastSlash == std::string::npos) {
					reply.status = eHTTPStatusCode::BAD_REQUEST;
					reply.message = "{\"error\":\"Invalid account ID\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				std::string idStr = path.substr(lastSlash + 1);
				uint32_t accountId = 0;
				try {
					accountId = std::stoul(idStr);
				} catch (...) {
					reply.status = eHTTPStatusCode::BAD_REQUEST;
					reply.message = "{\"error\":\"Invalid account ID\"}";
					reply.contentType = eContentType::APPLICATION_JSON;
					return;
				}

				// Permission check: GM 0 can only view own account, GM > 0 can view any account
				if (context.gmLevel == 0) {
					// Regular user - get their own account ID
					auto currentUserInfo = Database::Get()->GetAccountInfo(context.authenticatedUser);
					if (!currentUserInfo.has_value() || currentUserInfo->id != accountId) {
						reply.status = eHTTPStatusCode::FORBIDDEN;
						reply.message = "{\"error\":\"Forbidden - You do not have permission to view this account\"}";
						reply.contentType = eContentType::APPLICATION_JSON;
						return;
					}
				}

				// Get account data
			nlohmann::json response = Database::Get()->GetAccountById(accountId);
				reply.status = eHTTPStatusCode::OK;
				reply.message = response.dump();
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (const nlohmann::json::exception& jsonEx) {
				LOG("JSON error in /api/accounts/:id: %s", jsonEx.what());
				reply.status = eHTTPStatusCode::BAD_REQUEST;
				reply.message = "{\"error\":\"Invalid JSON\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			} catch (std::exception& ex) {
				LOG("Error in /api/accounts/:id: %s", ex.what());
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.message = "{\"error\":\"Database error\"}";
				reply.contentType = eContentType::APPLICATION_JSON;
			}
		}
	});
}

