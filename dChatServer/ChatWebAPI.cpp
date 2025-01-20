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
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> HTTPRoutes {};
	std::map<std::string, WSAction> WSactions {};
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

void HandleHTTPPlayersRequest(HTTPReply& reply, std::string body) {
	const json data = Game::playerContainer;
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Players Online\"}" : data.dump();
}

void HandleHTTPTeamsRequest(HTTPReply& reply, std::string body) {
	const json data = Game::playerContainer.GetTeamContainer();
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Teams Online\"}" : data.dump();
}

void HandleHTTPAnnounceRequest(HTTPReply& reply, std::string body) {
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

void HandleHTTPInvalidRoute(HTTPReply& reply) {
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

		// Special case for websocket
		if (uri == "/ws" && method == eHTTPMethod::GET) {
			mg_ws_upgrade(connection, const_cast<mg_http_message*>(http_msg), NULL);
			return;
		}

		const auto routeItr = HTTPRoutes.find({method, uri});
		if (routeItr != HTTPRoutes.end()) {
			const auto& [_, route] = *routeItr;
			route.handle(reply, body);
		} else HandleHTTPInvalidRoute(reply);
	} else {
		reply.status = eHTTPStatusCode::UNAUTHORIZED;
		reply.message = "{\"error\":\"Unauthorized\"}";
	}
	mg_http_reply(connection, static_cast<int>(reply.status), json_content_type, reply.message.c_str());
}

void HandleWSAnnounce(json data){
	auto check = JSONUtils::CheckRequiredData(data, { "title", "message" });
	if (!check.empty()) {
		LOG("Received invalid websocket message: %s", check.c_str());
	} else {
		ChatPackets::Announcement announcement;
		announcement.title = data["title"];
		announcement.message = data["message"];
		announcement.Send();
	}
}

void HandleWSChat(json data) {
	auto check = JSONUtils::CheckRequiredData(data, { "user", "message" });
	if (!check.empty()) {
		LOG("Received invalid websocket message: %s", check.c_str());
	} else {
		const auto user = data["user"].get<std::string>();
		const auto message = data["message"].get<std::string>();
		LOG("EXTERNAL Chat message from %s: %s", user.c_str(), message.c_str());
		//TODO: Send chat message to  corret world server to broadcast to players
	}
}

void HandleWSMessage(mg_connection* connection, const mg_ws_message* ws_msg) {
	std::string reply = "{\"status\":\"Error\"}";
	if (!ws_msg) {
		LOG("Received invalid websocket message");
		return;
	} else {
		LOG("Received websocket message: %.*s", static_cast<uint32_t>(ws_msg->data.len), ws_msg->data.buf);
		auto data = GeneralUtils::TryParse<json>(std::string(ws_msg->data.buf, ws_msg->data.len));
		if (data) {
			const auto& good_data = data.value();
			auto check = JSONUtils::CheckRequiredData(good_data, { "action" });
			if (!check.empty()) {
				LOG("Received invalid websocket message: %s", check.c_str());
				reply = "{\"status\":\"no action\"}";
			} else {
				const auto action = good_data["action"].get<std::string>();
				const auto actionItr = WSactions.find(action);
				if (actionItr != WSactions.end()) {
					const auto& [_, action] = *actionItr;
					action.handle(good_data);
					reply = "{\"status\":\"OK\"}";
				} else {
					LOG("Received invalid websocket action: %s", action.c_str());
					reply = "{\"status\":\"invalid action\"}";
				}
			}
		} else {
			LOG("Received invalid websocket message: %.*s", static_cast<uint32_t>(ws_msg->data.len), ws_msg->data.buf);
		}
	}
	mg_ws_send(connection, reply.c_str(), reply.size(), WEBSOCKET_OP_TEXT);
}



void HandleMessages(mg_connection* connection, int message, void* message_data) {
	switch (message) {
		case MG_EV_HTTP_MSG:
			HandleHTTPMessage(connection, static_cast<mg_http_message*>(message_data));
			break;
		case MG_EV_WS_MSG:
			HandleWSMessage(connection, static_cast<mg_ws_message*>(message_data));
			break;
		default:
			break;
	}
}

void ChatWebAPI::RegisterHTTPRoute(HTTPRoute route) {
	auto [_, success] = HTTPRoutes.try_emplace({ route.method, route.path }, route);
	if (!success) {
		LOG_DEBUG("Failed to register HTTP route %s", route.path.c_str());
	} else {
		LOG_DEBUG("Registered HTTP route %s", route.path.c_str());
	}
}

void ChatWebAPI::RegisterWSAction(WSAction action) {
	auto [_, success] = WSactions.try_emplace(action.action, action);
	if (!success) {
		LOG_DEBUG("Failed to register WS action %s", action.action.c_str());
	} else {
		LOG_DEBUG("Registered WS action %s", action.action.c_str());
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
	std::string listen_ip = Game::config->GetValue("web_server_listen_ip");
	if (listen_ip == "localhost") listen_ip = "127.0.0.1";

	const std::string& listen_port = Game::config->GetValue("web_server_listen_port");
	const std::string& listen_address = "http://" + listen_ip + ":" + listen_port;
	LOG("Starting web server on %s", listen_address.c_str());

	// Create HTTP listener
	if (!mg_http_listen(&mgr, listen_address.c_str(), HandleMessages, NULL)) {
		LOG("Failed to create web server listener on %s", listen_port.c_str());
		return false;
	}

	// Register routes

	// API v1 routes
	std::string v1_route = "/api/v1/";
	RegisterHTTPRoute({
		.path = v1_route + "players",
		.method = eHTTPMethod::GET,
		.handle = HandleHTTPPlayersRequest
	});

	RegisterHTTPRoute({
		.path = v1_route + "teams",
		.method = eHTTPMethod::GET,
		.handle = HandleHTTPTeamsRequest
	});

	RegisterHTTPRoute({
		.path = v1_route + "announce",
		.method = eHTTPMethod::POST,
		.handle = HandleHTTPAnnounceRequest
	});

	// WebSocket Actions
	RegisterWSAction({
		.action = "announce",
		.handle = HandleWSAnnounce
	});

	RegisterWSAction({
		.action = "chat",
		.handle = HandleWSChat
	});


	return true;
}

void ChatWebAPI::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}

void ChatWebAPI::SendWSMessage(const std::string& message) {
	for (struct mg_connection *wc = mgr.conns; wc != NULL; wc = wc->next) {
		if (wc->is_websocket) {
			mg_ws_send(wc, message.c_str(), message.size(), WEBSOCKET_OP_TEXT);
		}
	}
}

#ifdef DARKFLAME_PLATFORM_WIN32
#pragma pop_macro("DELETE")
#endif
