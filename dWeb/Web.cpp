#include "Web.h"
#include "Game.h"
#include "magic_enum.hpp"
#include "json.hpp"
#include "Logger.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "JSONUtils.h"
#include <ranges>

namespace Game {
	Web web;
}

namespace {
	const char* jsonContentType = "Content-Type: application/json\r\n";
	const std::string wsSubscribed = "{\"status\":\"subscribed\"}";
	const std::string wsUnsubscribed = "{\"status\":\"unsubscribed\"}";
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> g_HTTPRoutes;
	std::map<std::string, WSEvent> g_WSEvents;
	std::vector<std::string> g_WSSubscriptions;
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
		// get method from mg to enum
		const eHTTPMethod method = magic_enum::enum_cast<eHTTPMethod>(method_string).value_or(eHTTPMethod::INVALID);

		// convert uri from cstring to std string
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);

		// convert body from cstring to std string
		std::string body(http_msg->body.buf, http_msg->body.len);

		// Special case for websocket
		if (uri == "/ws" && method == eHTTPMethod::GET) {
			mg_ws_upgrade(connection, const_cast<mg_http_message*>(http_msg), NULL);
			LOG_DEBUG("Upgraded connection to websocket: %d.%d.%d.%d:%i", MG_IPADDR_PARTS(&connection->rem.ip), connection->rem.port);
			// return cause they are now a websocket
			return;
		}

		// Handle HTTP request
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
	mg_http_reply(connection, static_cast<int>(reply.status), jsonContentType, reply.message.c_str());
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
			auto check = JSONUtils::CheckRequiredData(good_data, { "event" });
			if (!check.empty()) {
				LOG_DEBUG("Received invalid websocket message: %s", check.c_str());
			} else {
				const auto event = good_data["event"].get<std::string>();
				const auto eventItr = g_WSEvents.find(event);
				if (eventItr != g_WSEvents.end()) {
					const auto& [_, event] = *eventItr;
					event.handle(connection, good_data);
				} else {
					LOG_DEBUG("Received invalid websocket event: %s", event.c_str());
				}
			}
		} else {
			LOG_DEBUG("Received invalid websocket message: %.*s", static_cast<uint32_t>(ws_msg->data.len), ws_msg->data.buf);
		}
	}
}

// Handle websocket connection subscribing to an event
void HandleWSSubscribe(mg_connection* connection, json data) {
	auto check = JSONUtils::CheckRequiredData(data, { "subscription" });
	if (!check.empty()) {
		LOG_DEBUG("Received invalid websocket message: %s", check.c_str());
	} else {
		const auto subscription = data["subscription"].get<std::string>();
		// check subscription vector
		auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
		if (subItr != g_WSSubscriptions.end()) {
			// get index of subscription
			auto index = std::distance(g_WSSubscriptions.begin(), subItr);
			connection->data[index] = SubscriptionStatus::SUBSCRIBED;
			// send subscribe message
			mg_ws_send(connection, wsSubscribed.c_str(), wsSubscribed.size(), WEBSOCKET_OP_TEXT);
			LOG_DEBUG("subscription %s subscribed", subscription.c_str());
		}
	}
}

// Handle websocket connection unsubscribing from an event
void HandleWSUnsubscribe(mg_connection* connection, json data) {
	auto check = JSONUtils::CheckRequiredData(data, { "subscription" });
	if (!check.empty()) {
		LOG_DEBUG("Received invalid websocket message: %s", check.c_str());
	} else {
		const auto subscription = data["subscription"].get<std::string>();
		// check subscription vector
		auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
		if (subItr != g_WSSubscriptions.end()) {
			// get index of subscription
			auto index = std::distance(g_WSSubscriptions.begin(), subItr);
			connection->data[index] = SubscriptionStatus::UNSUBSCRIBED;
			// send unsubscribe message
			mg_ws_send(connection, wsUnsubscribed.c_str(), wsUnsubscribed.size(), WEBSOCKET_OP_TEXT);
			LOG_DEBUG("subscription %s unsubscribed", subscription.c_str());
		}
	}
}

void HandleWSGetSubscriptions(mg_connection* connection, json data) {
	// list subscribed and non subscribed subscriptions
	json response;
	// check subscription vector
	for (const auto& sub : g_WSSubscriptions) {
		auto subItr = std::ranges::find(g_WSSubscriptions, sub);
		if (subItr != g_WSSubscriptions.end()) {
			// get index of subscription
			auto index = std::distance(g_WSSubscriptions.begin(), subItr);
			if (connection->data[index] == SubscriptionStatus::SUBSCRIBED) {
				response["subscribed"].push_back(sub);
			} else {
				response["unsubscribed"].push_back(sub);
			}
		}
	}
	mg_ws_send(connection, response.dump().c_str(), response.dump().size(), WEBSOCKET_OP_TEXT);
}

void HandleMessages(mg_connection* connection, int message, void* message_data) {
	if (!Game::web.IsEnabled()) return;
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

// Redirect mongoose logs to our logger
static void DLOG(char ch, void *param) {
	static char buf[256]{};
	static size_t len{};
	if (ch != '\n') buf[len++] = ch; // we provide the newline in our logger
	if (ch == '\n' || len >= sizeof(buf)) {
		if (Game::logger) LOG_DEBUG("%.*s", static_cast<int>(len), buf);
		len = 0;
	}
}

void Web::RegisterHTTPRoute(HTTPRoute route) {
	if (!Game::web.enabled) {
		LOG_DEBUG("Failed to register HTTP route %s: web server not enabled", route.path.c_str());
		return;
	}

	auto [_, success] = g_HTTPRoutes.try_emplace({ route.method, route.path }, route);
	if (!success) {
		LOG_DEBUG("Failed to register HTTP route %s", route.path.c_str());
	} else {
		LOG_DEBUG("Registered HTTP route %s", route.path.c_str());
	}
}

void Web::RegisterWSEvent(WSEvent event) {
	if (!Game::web.enabled) {
		LOG_DEBUG("Failed to register WS event %s: web server not enabled", event.name.c_str());
		return;
	}

	auto [_, success] = g_WSEvents.try_emplace(event.name, event);
	if (!success) {
		LOG_DEBUG("Failed to register WS event %s", event.name.c_str());
	} else {
		LOG_DEBUG("Registered WS event %s", event.name.c_str());
	}
}

void Web::RegisterWSSubscription(const std::string& subscription) {
	if (!Game::web.enabled) {
		LOG_DEBUG("Failed to register WS subscription %s: web server not enabled", subscription.c_str());
		return;
	}

	// check that subsction is not already in the vector
	auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
	if (subItr != g_WSSubscriptions.end()) {
		LOG_DEBUG("Failed to register WS subscription %s: duplicate", subscription.c_str());
	} else {
		LOG_DEBUG("Registered WS subscription %s", subscription.c_str());
		g_WSSubscriptions.push_back(subscription);
	}
}

Web::Web() {
	mg_log_set_fn(DLOG, NULL); // Redirect logs to our logger
	mg_log_set(MG_LL_DEBUG);
	mg_mgr_init(&mgr); // Initialize event manager
}

Web::~Web() {
	mg_mgr_free(&mgr);
}

bool Web::Startup(const std::string& listen_ip, const uint32_t listen_port) {

	// Make listen address
	const std::string listen_address = "http://" + listen_ip + ":" + std::to_string(listen_port);
	LOG("Starting web server on %s", listen_address.c_str());

	// Create HTTP listener
	if (!mg_http_listen(&mgr, listen_address.c_str(), HandleMessages, NULL)) {
		LOG("Failed to create web server listener on %s", listen_address.c_str());
		return false;
	}

	// Set enabled flag
	Game::web.enabled = true;
	
	// Core WebSocket Events
	Game::web.RegisterWSEvent({
		.name = "subscribe",
		.handle = HandleWSSubscribe
	});

	Game::web.RegisterWSEvent({
		.name = "unsubscribe",
		.handle = HandleWSUnsubscribe
	});

	Game::web.RegisterWSEvent({
		.name = "getSubscriptions",
		.handle = HandleWSGetSubscriptions
	});

	return true;
}

void Web::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}

void Web::SendWSMessage(const std::string subscription, json& data) {
	if (!Game::web.enabled) return; // don't attempt to send if web is not enabled

	// find subscription
	auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
	if (subItr == g_WSSubscriptions.end()) {
		LOG_DEBUG("Failed to send WS message: subscription %s not found", subscription.c_str());
		return;
	}
	// tell it the event type
	data["event"] = subscription;
	auto index = std::distance(g_WSSubscriptions.begin(), subItr);
	for (auto *wc = Game::web.mgr.conns; wc != NULL; wc = wc->next) {
		if (wc->is_websocket && wc->data[index] == SubscriptionStatus::SUBSCRIBED) {
			mg_ws_send(wc, data.dump().c_str(), data.dump().size(), WEBSOCKET_OP_TEXT);
		}
	}
}
