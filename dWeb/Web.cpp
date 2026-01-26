#include "Web.h"
#include "Game.h"
#include "magic_enum.hpp"
#include "json.hpp"
#include "Logger.h"
#include "eHTTPMethod.h"
#include "GeneralUtils.h"
#include "JSONUtils.h"
#include "HTTPContext.h"
#include "IHTTPMiddleware.h"
#include <ranges>
#include <set>
#include <cctype>

namespace Game {
	Web web;
}

namespace {
	const std::string wsSubscribed = "{\"status\":\"subscribed\"}";
	const std::string wsUnsubscribed = "{\"status\":\"unsubscribed\"}";
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> g_HTTPRoutes;
	std::map<std::string, WSEvent> g_WSEvents;
	std::vector<std::string> g_WSSubscriptions;
	// Keep track of authenticated WebSocket connections
	std::set<mg_connection*> g_AuthenticatedWSConnections;
	
	// Global middleware applied to all routes
	std::vector<MiddlewarePtr> g_GlobalMiddleware;
	
	// Helper to extract client IP from mongoose connection
	static std::string GetClientIP(mg_connection* connection) {
		if (!connection) return "unknown";
		
		const uint8_t* ip = connection->rem.ip;
		
		// Check for IPv4-mapped IPv6 addresses (::ffff:x.x.x.x)
		if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0 &&
			ip[4] == 0 && ip[5] == 0 && ip[6] == 0 && ip[7] == 0 &&
			ip[8] == 0 && ip[9] == 0 && ip[10] == 0xff && ip[11] == 0xff) {
			// IPv4 address is in bytes 12-15
			char buffer[32]{};
			snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d",
				ip[12], ip[13], ip[14], ip[15]);
			return buffer;
		}
		
		// Direct IPv4
		char buffer[32]{};
		snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d",
			ip[0], ip[1], ip[2], ip[3]);
		return buffer;
	}
	
	// Helper to populate HTTPContext from mg_http_message
	static void PopulateHTTPContext(HTTPContext& context, 
									const mg_http_message* http_msg,
									mg_connection* connection) {
		// Parse method
		context.method = std::string(http_msg->method.buf, http_msg->method.len);
		
		// Parse URI/path
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);
		
		// Split path and query string
		const size_t queryPos = uri.find('?');
		if (queryPos != std::string::npos) {
			context.path = uri.substr(0, queryPos);
			context.queryString = uri.substr(queryPos + 1);
		} else {
			context.path = uri;
			context.queryString = "";
		}
		
		// Parse body
		context.body = std::string(http_msg->body.buf, http_msg->body.len);
		
		// Parse common headers (case-insensitive)
		const struct mg_str* hdr_ptr;
		
		// Get Content-Type
		if ((hdr_ptr = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Content-Type")) != NULL) {
			context.SetHeader("Content-Type", std::string(hdr_ptr->buf, hdr_ptr->len));
		}
		
		// Get Cookie
		if ((hdr_ptr = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Cookie")) != NULL) {
			context.SetHeader("Cookie", std::string(hdr_ptr->buf, hdr_ptr->len));
		}
		
		// Get Authorization
		if ((hdr_ptr = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Authorization")) != NULL) {
			context.SetHeader("Authorization", std::string(hdr_ptr->buf, hdr_ptr->len));
		}
		
		// Get User-Agent
		if ((hdr_ptr = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "User-Agent")) != NULL) {
			context.SetHeader("User-Agent", std::string(hdr_ptr->buf, hdr_ptr->len));
		}
		
		// Get Host
		if ((hdr_ptr = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Host")) != NULL) {
			context.SetHeader("Host", std::string(hdr_ptr->buf, hdr_ptr->len));
		}
		
		// Get client IP
		context.clientIP = GetClientIP(connection);
	}

	const char* ContentTypeToString(eContentType contentType) {
		switch (contentType) {
			case eContentType::APPLICATION_JSON:
				return "application/json";
			case eContentType::TEXT_HTML:
				return "text/html; charset=utf-8";
			case eContentType::TEXT_CSS:
				return "text/css; charset=utf-8";
			case eContentType::TEXT_JAVASCRIPT:
				return "application/javascript; charset=utf-8";
			case eContentType::TEXT_PLAIN:
				return "text/plain; charset=utf-8";
			case eContentType::IMAGE_PNG:
				return "image/png";
			case eContentType::IMAGE_JPEG:
				return "image/jpeg";
			case eContentType::APPLICATION_OCTET_STREAM:
				return "application/octet-stream";
			default:
				return "application/json";
		}
	}
}

using json = nlohmann::json;

void HandleHTTPMessage(mg_connection* connection, const mg_http_message* http_msg) {
	if (g_HTTPRoutes.empty()) return;

	HTTPReply reply;
	
	if (!http_msg) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid Request\"}";
	} else {
		// All authentication is now handled by middleware chain
		// Convert method from cstring to enum
		std::string method_string(http_msg->method.buf, http_msg->method.len);
		const eHTTPMethod method = magic_enum::enum_cast<eHTTPMethod>(method_string).value_or(eHTTPMethod::INVALID);

		// Extract URI and convert to lowercase
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);

		// Special case for websocket
		if (uri == "/ws" && method == eHTTPMethod::GET) {
			// Check if connection is from localhost/internal network
			bool isInternal = false;
			const uint8_t* ip = connection->rem.ip;
			
			// Check for IPv4-mapped IPv6 addresses (::ffff:x.x.x.x)
			if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0 &&
				ip[4] == 0 && ip[5] == 0 && ip[6] == 0 && ip[7] == 0 &&
				ip[8] == 0 && ip[9] == 0 && ip[10] == 0xff && ip[11] == 0xff) {
				// IPv4 address is in bytes 12-15
				uint8_t b1 = ip[12];
				uint8_t b2 = ip[13];
				
				// Check for 127.x.x.x (localhost)
				if (b1 == 127) {
					isInternal = true;
				}
				// Check for 192.168.x.x
				else if (b1 == 192 && b2 == 168) {
					isInternal = true;
				}
				// Check for 10.x.x.x
				else if (b1 == 10) {
					isInternal = true;
				}
				// Check for 172.16.x.x to 172.31.x.x
				else if (b1 == 172 && b2 >= 16 && b2 <= 31) {
					isInternal = true;
				}
			}
			
			bool authenticated = isInternal; // Internal connections are automatically trusted
			
			// For external connections, require authentication cookie
			if (!isInternal) {
				const auto* cookieHeader = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Cookie");
				if (cookieHeader) {
					std::string cookieStr = std::string(cookieHeader->buf, cookieHeader->len);
					if (!cookieStr.empty() && cookieStr.find("dashboardToken=") != std::string::npos) {
						authenticated = true;
					}
				}
			}
			
			if (authenticated) {
				mg_ws_upgrade(connection, const_cast<mg_http_message*>(http_msg), NULL);
				g_AuthenticatedWSConnections.insert(connection);
				const char* connType = isInternal ? "internal" : "external";
				LOG_DEBUG("Upgraded %s connection to websocket: %d.%d.%d.%d:%i", connType, MG_IPADDR_PARTS(&connection->rem.ip), connection->rem.port);
			} else {
				LOG_DEBUG("Rejected WebSocket connection - no valid authentication from %d.%d.%d.%d:%i", MG_IPADDR_PARTS(&connection->rem.ip), connection->rem.port);
				reply.status = eHTTPStatusCode::UNAUTHORIZED;
				reply.message = "{\"error\":\"Unauthorized\"}";
				std::string headers = std::string("Content-Type: ") + ContentTypeToString(reply.contentType) + "\r\n";
				if (!reply.location.empty()) {
					headers += "Location: " + reply.location + "\r\n";
				}
				mg_http_reply(connection, static_cast<int>(reply.status), headers.c_str(), reply.message.c_str());
			}
			// return cause they are now a websocket or connection closed
			return;
		}

		// Handle HTTP request
		const auto routeItr = g_HTTPRoutes.find({method, uri});
		if (routeItr != g_HTTPRoutes.end()) {
			const auto& route = routeItr->second;
			
			// Create HTTP context from request
			HTTPContext context;
			PopulateHTTPContext(context, http_msg, connection);
			
			// Build complete middleware chain
			std::vector<MiddlewarePtr> middlewareChain = g_GlobalMiddleware;
			middlewareChain.insert(middlewareChain.end(), 
								   route.middleware.begin(), 
								   route.middleware.end());
			
			// Execute middleware chain
			bool chainPassed = true;
			for (const auto& middleware : middlewareChain) {
				if (!middleware->Process(context, reply)) {
					chainPassed = false;
					LOG_DEBUG("Middleware %s rejected request to %s %s", 
							  middleware->GetName().c_str(),
							  context.method.c_str(),
							  context.path.c_str());
					break;
				}
			}
			
			// Call handler only if all middleware passed
			if (chainPassed) {
				route.handle(reply, context);
			}
		} else {
			reply.status = eHTTPStatusCode::NOT_FOUND;
			reply.message = "{\"error\":\"Not Found\"}";
		}
	}
	
	// Build headers
	std::string headers = std::string("Content-Type: ") + ContentTypeToString(reply.contentType) + "\r\n";
	if (!reply.location.empty()) {
		headers += "Location: " + reply.location + "\r\n";
	}
	mg_http_reply(connection, static_cast<int>(reply.status), headers.c_str(), reply.message.c_str());
}



void HandleWSMessage(mg_connection* connection, const mg_ws_message* ws_msg) {
	// Check if connection is authenticated
	if (g_AuthenticatedWSConnections.find(connection) == g_AuthenticatedWSConnections.end()) {
		LOG_DEBUG("Received websocket message from unauthenticated connection");
		mg_ws_send(connection, "{\"error\":\"Unauthorized\"}", 23, WEBSOCKET_OP_TEXT);
		return;
	}
	
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

void Web::AddGlobalMiddleware(MiddlewarePtr middleware) {
	if (!middleware) {
		LOG_DEBUG("Attempted to add null middleware");
		return;
	}
	g_GlobalMiddleware.push_back(middleware);
	LOG_DEBUG("Registered global middleware: %s", middleware->GetName().c_str());
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
	
	// Clean up closed connections from authenticated set
	std::vector<mg_connection*> closedConnections;
	for (auto* conn : g_AuthenticatedWSConnections) {
		if (conn->is_closing) {
			closedConnections.push_back(conn);
		}
	}
	for (auto* conn : closedConnections) {
		g_AuthenticatedWSConnections.erase(conn);
	}
	
	for (auto *wc = Game::web.mgr.conns; wc != NULL; wc = wc->next) {
		if (wc->is_websocket && wc->data[index] == SubscriptionStatus::SUBSCRIBED) {
			mg_ws_send(wc, data.dump().c_str(), data.dump().size(), WEBSOCKET_OP_TEXT);
		}
	}
}
