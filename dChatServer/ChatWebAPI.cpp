#include "ChatWebAPI.h"

#include "Logger.h"
#include "Game.h"
#include "json.hpp"
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "dServer.h"
#include "dConfig.h"
#include "PlayerContainer.h"
#include "JSONUtils.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"
#include "ChatPackets.h"
#include "StringifiedEnum.h"
#include "Database.h"

#ifdef DARKFLAME_PLATFORM_WIN32
#pragma push_macro("DELETE")
#undef DELETE
#endif

using json = nlohmann::json;

typedef struct mg_connection mg_connection;
typedef struct mg_http_message mg_http_message;

namespace {
	const char* json_content_type = "Content-Type: application/json\r\n";
	std::map<std::pair<eHTTPMethod, std::string>, WebAPIHTTPRoute> Routes {};
}

bool ValidateAuthentication(const mg_http_message* http_msg) {
	// TO DO: This is just a placeholder for now
	// use tokens or something at a later point if we want to implement authentication
	// bit using the listen bind address to limit external access is good enough to start with
	return true;
}

bool ValidateJSON(std::optional<json> data, HTTPReply& reply) {
	if (!data) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid JSON\"}";
		return false;
	}
	return true;
}

void HandlePlayersRequest(HTTPReply& reply, std::string body) {
	const json data = Game::playerContainer;
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Players Online\"}" : data.dump();
}

void HandleTeamsRequest(HTTPReply& reply, std::string body) {
	const json data = Game::playerContainer.GetTeamContainer();
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Teams Online\"}" : data.dump();
}

void HandleAnnounceRequest(HTTPReply& reply, std::string body) {
	auto data = GeneralUtils::TryParse<json>(body);
	if (!ValidateJSON(data, reply)) return;

	const auto& good_data = data.value();
	auto check = JSONUtils::CheckRequiredData(good_data, { "title", "message" });
	if (!check.empty()) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = check;
	} else {

		ChatPackets::Announcement announcement;
		announcement.title = good_data["title"];
		announcement.message = good_data["message"];
		announcement.Send();

		reply.status = eHTTPStatusCode::OK;
		reply.message = "{\"status\":\"Announcement Sent\"}";
	}
}

void HandleInvalidRoute(HTTPReply& reply) {
	reply.status = eHTTPStatusCode::NOT_FOUND;
	reply.message = "{\"error\":\"Invalid Route\"}";
}

void HandleHTTPMessage(mg_connection* connection, const mg_http_message* http_msg) {
	HTTPReply reply;
	
	if (!http_msg) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid Request\"}";
	} else if (ValidateAuthentication(http_msg)) {
		
		// convert method from cstring to std string
		std::string method_string(http_msg->method.buf, http_msg->method.len);
		// get mehtod from mg to enum
		const eHTTPMethod method = magic_enum::enum_cast<eHTTPMethod>(method_string).value_or(eHTTPMethod::INVALID);

		// convert uri from cstring to std string
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);

		// convert body from cstring to std string
		std::string body(http_msg->body.buf, http_msg->body.len);

		
		const auto routeItr = Routes.find({method, uri});

		if (routeItr != Routes.end()) {
			const auto& [_, route] = *routeItr;
			route.handle(reply, body);
		} else HandleInvalidRoute(reply);
	} else {
		reply.status = eHTTPStatusCode::UNAUTHORIZED;
		reply.message = "{\"error\":\"Unauthorized\"}";
	}
	mg_http_reply(connection, static_cast<int>(reply.status), json_content_type, reply.message.c_str());
}


void HandleRequests(mg_connection* connection, int request, void* request_data) {
	switch (request) {
		case MG_EV_HTTP_MSG:
			HandleHTTPMessage(connection, static_cast<mg_http_message*>(request_data));
			break;
		default:
			break;
	}
}

void ChatWebAPI::RegisterHTTPRoutes(WebAPIHTTPRoute route) {
	auto [_, success] = Routes.try_emplace({ route.method, route.path }, route);
	if (!success) {
		LOG_DEBUG("Failed to register route %s", route.path.c_str());
	} else {
		LOG_DEBUG("Registered route %s", route.path.c_str());
	}
}

ChatWebAPI::ChatWebAPI() {
	mg_log_set(MG_LL_NONE);
	mg_mgr_init(&mgr);  // Initialize event manager
}

ChatWebAPI::~ChatWebAPI() {
	mg_mgr_free(&mgr);
}

bool ChatWebAPI::Startup() {
	// Make listen address
	// std::string listen_ip = Game::config->GetValue("web_server_listen_ip");
	// if (listen_ip == "localhost") listen_ip = "127.0.0.1";

	const std::string& listen_port = Game::config->GetValue("web_server_listen_port");
	// const std::string& listen_address = "http://" + listen_ip + ":" + listen_port;
	const std::string& listen_address = "http://localhost:" + listen_port;
	LOG("Starting web server on %s", listen_address.c_str());

	// Create HTTP listener
	if (!mg_http_listen(&mgr, listen_address.c_str(), HandleRequests, NULL)) {
		LOG("Failed to create web server listener on %s", listen_port.c_str());
		return false;
	}

	// Register routes

	// API v1 routes
	std::string v1_route = "/api/v1/";
	RegisterHTTPRoutes({
		.path = v1_route + "players",
		.method = eHTTPMethod::GET,
		.handle = HandlePlayersRequest
	});

	RegisterHTTPRoutes({
		.path = v1_route + "teams",
		.method = eHTTPMethod::GET,
		.handle = HandleTeamsRequest
	});

	RegisterHTTPRoutes({
		.path = v1_route + "announce",
		.method = eHTTPMethod::POST,
		.handle = HandleAnnounceRequest
	});
	return true;
}

void ChatWebAPI::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}

#ifdef DARKFLAME_PLATFORM_WIN32
#pragma pop_macro("DELETE")
#endif
