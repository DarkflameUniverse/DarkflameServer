#include "Web.h"
#include "Game.h"
#include "magic_enum.hpp"
#include "json.hpp"
#include "Logger.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "JSONUtils.h"


namespace {
	const char * json_content_type = "application/json";
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> g_HTTPRoutes;
	std::map<std::string, WSAction> g_WSactions;
}

using json = nlohmann::json;

bool ValidateAuthentication(const mg_http_message* http_msg) {
	// TO DO: This is just a placeholder for now
	// use tokens or something at a later point if we want to implement authentication
	// bit using the listen bind address to limit external access is good enough to start with
	return true;
}

void HandleHTTPMessage(mg_connection* connection, const mg_http_message* http_msg) {
	if (g_HTTPRoutes.empty()) return;

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
			LOG("Upgraded connection to websocket: %d.%d.%d.%d:%i", MG_IPADDR_PARTS(&connection->rem.ip), connection->rem.port);
			return;
		}

		const auto routeItr = g_HTTPRoutes.find({method, uri});
		if (routeItr != g_HTTPRoutes.end()) {
			const auto& [_, route] = *routeItr;
			route.handle(reply, body);
		} else {
			reply.status = eHTTPStatusCode::NOT_FOUND;
			reply.message = "{\"error\":\"Not Found\"}";
		}
	} else {
		reply.status = eHTTPStatusCode::UNAUTHORIZED;
		reply.message = "{\"error\":\"Unauthorized\"}";
	}
	mg_http_reply(connection, static_cast<int>(reply.status), json_content_type, reply.message.c_str());
}


void HandleWSMessage(mg_connection* connection, const mg_ws_message* ws_msg) {
	if (!ws_msg) {
		LOG_DEBUG("Received invalid websocket message");
		return;
	} else {
		LOG_DEBUG("Received websocket message: %.*s", static_cast<uint32_t>(ws_msg->data.len), ws_msg->data.buf);
		auto data = GeneralUtils::TryParse<json>(std::string(ws_msg->data.buf, ws_msg->data.len));
		if (data) {
			const auto& good_data = data.value();
			auto check = JSONUtils::CheckRequiredData(good_data, { "action" });
			if (!check.empty()) {
				LOG_DEBUG("Received invalid websocket message: %s", check.c_str());
			} else {
				const auto action = good_data["action"].get<std::string>();
				const auto actionItr = g_WSactions.find(action);
				if (actionItr != g_WSactions.end()) {
					const auto& [_, action] = *actionItr;
					action.handle(connection, good_data);
				} else {
					LOG_DEBUG("Received invalid websocket action: %s", action.c_str());
				}
			}
		} else {
			LOG_DEBUG("Received invalid websocket message: %.*s", static_cast<uint32_t>(ws_msg->data.len), ws_msg->data.buf);
		}
	}
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

void Web::RegisterHTTPRoute(HTTPRoute route) {
	auto [_, success] = g_HTTPRoutes.try_emplace({ route.method, route.path }, route);
	if (!success) {
		LOG_DEBUG("Failed to register HTTP route %s", route.path.c_str());
	} else {
		LOG_DEBUG("Registered HTTP route %s", route.path.c_str());
	}
}

void Web::RegisterWSAction(WSAction action) {
	auto [_, success] = g_WSactions.try_emplace(action.action, action);
	if (!success) {
		LOG_DEBUG("Failed to register WS action %s", action.action.c_str());
	} else {
		LOG_DEBUG("Registered WS action %s", action.action.c_str());
	}
}

Web::Web() {
	mg_log_set(MG_LL_NONE);
	mg_mgr_init(&mgr);  // Initialize event manager
}

Web::~Web() {
	mg_mgr_free(&mgr);
}

bool Web::Startup(const std::string& listen_ip, const uint32_t listen_port) {
	// Make listen address
	const std::string& listen_address = "http://" + listen_ip + ":" + std::to_string(listen_port);
	LOG("Starting web server on %s", listen_address.c_str());

	// Create HTTP listener
	if (!mg_http_listen(&mgr, listen_address.c_str(), HandleMessages, NULL)) {
		LOG("Failed to create web server listener on %s", listen_address.c_str());
		return false;
	}	
	return true;
}

void Web::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}

void Web::SendWSMessage(const std::string subscription, const std::string& message) {
	for (struct mg_connection *wc = Game::web.mgr.conns; wc != NULL; wc = wc->next) {
		if (wc->is_websocket /* && wc->data[GeneralUtils::ToUnderlying(sub)] == 1*/) {
			mg_ws_send(wc, message.c_str(), message.size(), WEBSOCKET_OP_TEXT);
		}
	}
}