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
#include <vector>
#include <cctype>
#include <chrono>

namespace Game {
	Web web;
}

namespace {
	const std::string wsSubscribed = "{\"status\":\"subscribed\"}";
	const std::string wsUnsubscribed = "{\"status\":\"unsubscribed\"}";
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> g_HTTPRoutes;
	std::map<std::string, WSEvent> g_WSEvents;
	std::vector<std::string> g_WSSubscriptions;
	// Minimum permission level per subscription, parallel to g_WSSubscriptions
	std::vector<std::function<uint8_t()>> g_WSSubscriptionLevels;
	// Authenticated WebSocket connections: their permission level, account and the token they connected with.
	// Entries are removed on MG_EV_CLOSE so a reused connection address is never treated as authenticated.
	struct WSClient {
		uint8_t level{};
		uint32_t accountId{};
		std::string token; // empty for trusted internal connections, which are never rechecked
		bool apiToken{};   // connected with Authorization: Bearer (subject to the API access rule)
		std::chrono::steady_clock::time_point nextCheck;
	};
	std::map<mg_connection*, WSClient> g_AuthenticatedWSConnections;
	constexpr uint8_t INTERNAL_WS_LEVEL = UINT8_MAX;
	constexpr auto WS_RECHECK_INTERVAL = std::chrono::seconds(60);

	// Close a WebSocket whose session is no longer valid (logged out everywhere, banned, demoted below dashboard access)
	void CloseWebSocket(mg_connection* connection) {
		static const std::string ended = "{\"event\":\"session_ended\"}";
		mg_ws_send(connection, ended.c_str(), ended.size(), WEBSOCKET_OP_TEXT);
		mg_ws_send(connection, "", 0, WEBSOCKET_OP_CLOSE);
		connection->is_draining = 1;
		g_AuthenticatedWSConnections.erase(connection);
	}

	void RecheckDueWebSockets() {
		const auto& callback = Game::web.GetWSAuthCallback();
		if (!callback) return;
		const auto now = std::chrono::steady_clock::now();
		std::vector<mg_connection*> expired;
		for (auto& [connection, client] : g_AuthenticatedWSConnections) {
			if (client.token.empty() || client.nextCheck > now) continue;
			client.nextCheck = now + WS_RECHECK_INTERVAL;
			auto auth = callback(client.token);
			if (auth && client.apiToken && Game::web.GetWSApiAccessCallback() && !Game::web.GetWSApiAccessCallback()(auth->level)) auth.reset();
			if (!auth || auth->accountId != client.accountId) {
				expired.push_back(connection);
				continue;
			}
			client.level = auth->level;
		}
		for (auto* connection : expired) {
			LOG_DEBUG("Closing a WebSocket whose session is no longer valid");
			CloseWebSocket(connection);
		}
	}
	
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
		
		// Paths are matched case-insensitively. mongoose keeps the query string separate from the URI;
		// it is not lowercased because parameter values (search terms) are case-sensitive.
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		context.originalPath = uri;
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);
		context.path = uri;
		context.queryString = std::string(http_msg->query.buf, http_msg->query.len);

		// Parse body
		context.body = std::string(http_msg->body.buf, http_msg->body.len);

		// Copy every request header; HTTPContext lowercases names for case-insensitive lookup
		for (const auto& header : http_msg->headers) {
			if (header.name.len == 0) break;
			context.SetHeader(std::string(header.name.buf, header.name.len), std::string(header.value.buf, header.value.len));
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
			case eContentType::TEXT_CSV:
				return "text/csv; charset=utf-8";
			case eContentType::IMAGE_PNG:
				return "image/png";
			case eContentType::IMAGE_JPEG:
				return "image/jpeg";
			case eContentType::APPLICATION_OCTET_STREAM:
				return "application/octet-stream";
			case eContentType::TEXT_PROMETHEUS:
				return "text/plain; version=0.0.4; charset=utf-8";
			default:
				return "application/json";
		}
	}
}

using json = nlohmann::json;

namespace {
	const char* ReasonPhrase(int status) {
		switch (status) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 409: return "Conflict";
		case 413: return "Payload Too Large";
		case 429: return "Too Many Requests";
		case 500: return "Internal Server Error";
		case 503: return "Service Unavailable";
		default: return status < 400 ? "OK" : "Error";
		}
	}
}

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
			
			// Internal connections are only trusted when the server has no token authentication.
			// With authentication configured a reverse proxy would make every client look internal.
			std::optional<WSAuth> level;
			std::string connectToken;
			bool apiToken = false;
			if (!Game::web.GetWSAuthCallback()) {
				if (isInternal) level = WSAuth{ INTERNAL_WS_LEVEL, 0 };
			} else if (const auto* authHeader = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Authorization");
				authHeader && std::string_view(authHeader->buf, authHeader->len).starts_with("Bearer ")) {
				// Bots and scripts: an API token, like the REST API takes it (and subject to the same API access rule)
				const std::string token(authHeader->buf + 7, authHeader->len - 7);
				level = Game::web.GetWSAuthCallback()(token);
				if (level && Game::web.GetWSApiAccessCallback() && !Game::web.GetWSApiAccessCallback()(level->level)) level.reset();
				connectToken = token;
				apiToken = true;
			} else {
				const auto* cookieHeader = mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Cookie");
				if (cookieHeader) {
					std::string cookieStr = std::string(cookieHeader->buf, cookieHeader->len);
					
					// Extract token from cookie
					const std::string tokenPrefix = "dashboardToken=";
					const size_t tokenPos = cookieStr.find(tokenPrefix);
					
					if (tokenPos != std::string::npos) {
						size_t valueStart = tokenPos + tokenPrefix.length();
						size_t valueEnd = cookieStr.find(";", valueStart);
						
						if (valueEnd == std::string::npos) {
							valueEnd = cookieStr.length();
						}
						
						std::string token = cookieStr.substr(valueStart, valueEnd - valueStart);
						
						level = Game::web.GetWSAuthCallback()(token);
						connectToken = token;
					}
				}
			}
			
			if (level) {
				mg_ws_upgrade(connection, const_cast<mg_http_message*>(http_msg), NULL);
				g_AuthenticatedWSConnections[connection] = { level->level, level->accountId, connectToken, apiToken,
					std::chrono::steady_clock::now() + WS_RECHECK_INTERVAL };
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
		auto routeItr = g_HTTPRoutes.find({method, uri});
		
		// If exact match not found, try pattern matching with :param syntax
		if (routeItr == g_HTTPRoutes.end()) {
			for (const auto& [key, route] : g_HTTPRoutes) {
				if (key.first != method) continue;
				
				const std::string& pattern = key.second;
				// Simple pattern matching for :param syntax
				if (pattern.find(':') != std::string::npos) {
					// Split by '/' and compare segments
					std::vector<std::string> patternSegments;
					std::vector<std::string> uriSegments;
					
					size_t pos = 0;
					const std::string& str = pattern;
					while (pos < str.length()) {
						size_t slash = str.find('/', pos);
						if (slash == std::string::npos) slash = str.length();
						if (slash > pos) { // Skip empty segments
							patternSegments.push_back(str.substr(pos, slash - pos));
						}
						pos = slash + 1;
					}
					
					pos = 0;
					while (pos < uri.length()) {
						size_t slash = uri.find('/', pos);
						if (slash == std::string::npos) slash = uri.length();
						if (slash > pos) { // Skip empty segments
							uriSegments.push_back(uri.substr(pos, slash - pos));
						}
						pos = slash + 1;
					}
					
					// Check if segment counts match
					if (patternSegments.size() == uriSegments.size()) {
						bool matches = true;
						for (size_t i = 0; i < patternSegments.size(); ++i) {
							const auto& patternSeg = patternSegments[i];
							const auto& uriSeg = uriSegments[i];
							
							// If pattern segment is a parameter (starts with :), it always matches
							// Otherwise it must be an exact match
							if (!patternSeg.empty() && patternSeg[0] != ':' && patternSeg != uriSeg) {
								matches = false;
								break;
							}
						}
						
						if (matches) {
							routeItr = g_HTTPRoutes.find({method, pattern});
							break;
						}
					}
				}
			}
		}
		
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
			
			// Call handler only if all middleware passed. A failing handler (e.g. a database error) answers 500
			// instead of taking the whole server down.
			if (chainPassed) {
				try {
					route.handle(reply, context);
				} catch (const std::exception& ex) {
					LOG("Error handling %s %s: %s", context.method.c_str(), context.path.c_str(), ex.what());
					reply = HTTPReply{};
					reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
					reply.message = "{\"success\":false,\"error\":\"Internal server error\"}";
				}
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
	// A route's own header replaces a default header of the same name
	const auto headerName = [](const std::string& header) {
		std::string name = header.substr(0, header.find(':'));
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);
		return name;
	};
	for (const auto& header : Game::web.GetDefaultHeaders()) {
		const auto name = headerName(header);
		if (std::ranges::none_of(reply.headers, [&](const std::string& h) { return headerName(h) == name; })) headers += header + "\r\n";
	}
	for (const auto& header : reply.headers) headers += header + "\r\n";

	if (!reply.file.empty() && reply.status == eHTTPStatusCode::OK && http_msg) {
		// Streamed in chunks by mongoose. Content-Type comes from the mime override (it adds its own header).
		std::string extraHeaders = headers.substr(headers.find("\r\n") + 2);
		const std::string mimeTypes = std::string("*=") + ContentTypeToString(reply.contentType);
		mg_http_serve_opts opts{};
		opts.extra_headers = extraHeaders.c_str();
		opts.mime_types = mimeTypes.c_str();
		// Without Accept-Encoding, so a stale "<file>.gz" next to the file is never served instead
		mg_http_message request = *http_msg;
		for (auto& header : request.headers) {
			if (header.name.len && mg_strcasecmp(header.name, mg_str("Accept-Encoding")) == 0) header.name = mg_str("X-Ignored");
		}
		mg_http_serve_file(connection, &request, reply.file.c_str(), &opts);
		return;
	}

	// Written by hand rather than with mg_http_reply: that pads Content-Length with spaces (it fills the number in
	// afterwards), which strict clients such as Node's fetch reject, and it can't send binary bodies
	headers += "Content-Length: " + std::to_string(reply.message.size()) + "\r\n";
	const auto status = static_cast<int>(reply.status);
	std::string resp = "HTTP/1.1 " + std::to_string(status) + " " + ReasonPhrase(status) + "\r\n" + headers + "\r\n";
	mg_send(connection, resp.data(), resp.size());
	mg_send(connection, reply.message.data(), reply.message.size());
	connection->is_resp = 0;
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
					try {
						event.handle(connection, good_data);
					} catch (const std::exception& ex) {
						LOG("Error handling websocket event %s: %s", event.name.c_str(), ex.what());
					}
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
			const auto connItr = g_AuthenticatedWSConnections.find(connection);
			if (connItr == g_AuthenticatedWSConnections.end() || connItr->second.level < g_WSSubscriptionLevels[index]()) {
				const std::string forbidden = "{\"error\":\"Forbidden\",\"subscription\":\"" + subscription + "\"}";
				mg_ws_send(connection, forbidden.c_str(), forbidden.size(), WEBSOCKET_OP_TEXT);
				return;
			}
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
		case MG_EV_CLOSE:
			g_AuthenticatedWSConnections.erase(connection);
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

void Web::RegisterWSSubscription(const std::string& subscription, uint8_t minLevel) {
	RegisterWSSubscription(subscription, [minLevel] { return minLevel; });
}

void Web::RegisterWSSubscription(const std::string& subscription, std::function<uint8_t()> minLevel) {
	if (!Game::web.enabled) {
		LOG_DEBUG("Failed to register WS subscription %s: web server not enabled", subscription.c_str());
		return;
	}

	// check that subsction is not already in the vector
	auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
	if (subItr != g_WSSubscriptions.end()) {
		LOG_DEBUG("Failed to register WS subscription %s: duplicate", subscription.c_str());
	} else if (g_WSSubscriptions.size() >= MG_DATA_SIZE) {
		LOG("Failed to register WS subscription %s: limit of %d subscriptions reached", subscription.c_str(), MG_DATA_SIZE);
	} else {
		LOG_DEBUG("Registered WS subscription %s", subscription.c_str());
		g_WSSubscriptions.push_back(subscription);
		g_WSSubscriptionLevels.push_back(std::move(minLevel));
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

void Web::ReceiveRequests(int timeoutMs) {
	mg_mgr_poll(&mgr, timeoutMs);
	RecheckDueWebSockets();
}

void Web::RecheckWebSockets(uint32_t accountId) {
	for (auto& [connection, client] : g_AuthenticatedWSConnections) {
		if (accountId == 0 || client.accountId == accountId) client.nextCheck = {};
	}
}

void Web::SendWSMessageToAccount(const std::string subscription, json& data, uint32_t accountId) {
	if (!Game::web.enabled || accountId == 0) return;
	auto subItr = std::ranges::find(g_WSSubscriptions, subscription);
	if (subItr == g_WSSubscriptions.end()) return;
	data["event"] = subscription;
	const auto index = std::distance(g_WSSubscriptions.begin(), subItr);
	const auto payload = data.dump();
	const auto minLevel = g_WSSubscriptionLevels[index]();
	for (auto* wc = Game::web.GetManager().conns; wc != NULL; wc = wc->next) {
		if (!wc->is_websocket || wc->is_closing || wc->data[index] != SubscriptionStatus::SUBSCRIBED) continue;
		const auto connItr = g_AuthenticatedWSConnections.find(wc);
		if (connItr == g_AuthenticatedWSConnections.end() || connItr->second.accountId != accountId || connItr->second.level < minLevel) continue;
		mg_ws_send(wc, payload.c_str(), payload.size(), WEBSOCKET_OP_TEXT);
	}
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
	
	const auto payload = data.dump();
	const auto minLevel = g_WSSubscriptionLevels[index]();
	for (auto *wc = Game::web.GetManager().conns; wc != NULL; wc = wc->next) {
		if (!wc->is_websocket || wc->is_closing || wc->data[index] != SubscriptionStatus::SUBSCRIBED) continue;
		const auto connItr = g_AuthenticatedWSConnections.find(wc);
		if (connItr == g_AuthenticatedWSConnections.end() || connItr->second.level < minLevel) continue;
		mg_ws_send(wc, payload.c_str(), payload.size(), WEBSOCKET_OP_TEXT);
	}
}
