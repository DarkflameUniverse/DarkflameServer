#include "Profiler.h"
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
#include "Permissions.h"
#include <ranges>
#include <set>
#include <vector>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include "TrafficStats.h"

namespace Game {
	Web web;
}

namespace {
	const std::string wsSubscribed = "{\"status\":\"subscribed\"}";
	const std::string wsUnsubscribed = "{\"status\":\"unsubscribed\"}";
	std::map<std::pair<eHTTPMethod, std::string>, HTTPRoute> g_HTTPRoutes;
	std::map<std::pair<eHTTPMethod, std::string>, HTTPUploadRoute> g_UploadRoutes;
	std::map<std::string, WSEvent> g_WSEvents;
	std::vector<std::string> g_WSSubscriptions;
	// Minimum permission level per subscription, parallel to g_WSSubscriptions
	std::vector<std::function<uint8_t()>> g_WSSubscriptionLevels;
	// The permission guarding each subscription (empty: level only), parallel to g_WSSubscriptions
	std::vector<std::string> g_WSSubscriptionPermissions;
	// Authenticated WebSocket connections: their permission level, account and the token they connected with.
	// Entries are removed on MG_EV_CLOSE so a reused connection address is never treated as authenticated.
	struct WSClient {
		uint8_t level{};
		uint32_t accountId{};
		std::string token; // empty for trusted internal connections, which are never rechecked
		bool apiToken{};   // connected with Authorization: Bearer (subject to the API access rule)
		std::chrono::steady_clock::time_point nextCheck;
		std::shared_ptr<const ApiKeys::Scope> apiKey{}; // connected with an API key: its scope
		std::shared_ptr<const PermissionGrants::Held> grants{}; // the account's permission grants
	};

	constexpr uint8_t INTERNAL_WS_LEVEL = UINT8_MAX;

	// Whether a connection may subscribe to (and receive) a subscription
	bool MayReceive(const WSClient& client, size_t index, uint8_t minLevel) {
		const auto& permission = g_WSSubscriptionPermissions[index];
		// Guarded by a permission: its level or a grant (internal connections have every level)
		const bool allowed = permission.empty() || client.level == INTERNAL_WS_LEVEL ? client.level >= minLevel
			: Permissions::Allowed(client.level, permission, nullptr, client.grants.get());
		if (!allowed) return false;
		if (!client.apiKey) return true;
		return permission.empty() ? (minLevel == 0 || client.apiKey->allPermissions) : client.apiKey->Has(permission);
	}
	std::map<mg_connection*, WSClient> g_AuthenticatedWSConnections;
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
			if (auth && client.apiToken && Game::web.GetWSApiAccessCallback() && !Game::web.GetWSApiAccessCallback()(*auth)) auth.reset();
			if (!auth || auth->accountId != client.accountId) {
				expired.push_back(connection);
				continue;
			}
			client.level = auth->level;
			client.apiKey = auth->apiKey;
			client.grants = auth->grants;
		}
		for (auto* connection : expired) {
			LOG_DEBUG("Closing a WebSocket whose session is no longer valid");
			CloseWebSocket(connection);
		}
	}
	
	// Global middleware applied to all routes
	std::vector<MiddlewarePtr> g_GlobalMiddleware;

	// Requests answered later by Web::Defer
	DeferredQueue g_Deferred;
	
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
		context.connectionId = connection ? connection->id : 0;
	}

	// Whether a route pattern with :param segments matches a (lowercased) path
	bool PatternMatches(const std::string& pattern, const std::string& uri) {
		if (pattern.find(':') == std::string::npos) return false;
		const auto split = [](const std::string& text) {
			std::vector<std::string_view> segments;
			size_t pos = 0;
			while (pos < text.length()) {
				size_t slash = text.find('/', pos);
				if (slash == std::string::npos) slash = text.length();
				if (slash > pos) segments.emplace_back(text.data() + pos, slash - pos); // Skip empty segments
				pos = slash + 1;
			}
			return segments;
		};
		const auto patternSegments = split(pattern);
		const auto uriSegments = split(uri);
		if (patternSegments.size() != uriSegments.size()) return false;
		for (size_t i = 0; i < patternSegments.size(); ++i) {
			// A parameter (starts with :) always matches; anything else must be the same
			if (!patternSegments[i].empty() && patternSegments[i][0] != ':' && patternSegments[i] != uriSegments[i]) return false;
		}
		return true;
	}

	// The route for a method and (lowercased) path: the exact path, else the first pattern that matches
	template<typename Routes>
	auto FindRoute(Routes& routes, eHTTPMethod method, const std::string& uri) {
		auto it = routes.find({ method, uri });
		if (it != routes.end()) return it;
		for (it = routes.begin(); it != routes.end(); ++it) {
			if (it->first.first == method && PatternMatches(it->first.second, uri)) return it;
		}
		return routes.end();
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

// Send a reply; http_msg is the request (null for a deferred reply, whose request is gone)
static void SendReply(mg_connection* connection, const HTTPReply& reply, const mg_http_message* http_msg) {
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

	if (!reply.file.empty() && reply.status == eHTTPStatusCode::OK) {
		// Streamed in chunks by mongoose. Content-Type comes from the mime override (it adds its own header).
		std::string extraHeaders = headers.substr(headers.find("\r\n") + 2);
		const std::string mimeTypes = std::string("*=") + ContentTypeToString(reply.contentType);
		mg_http_serve_opts opts{};
		opts.extra_headers = extraHeaders.c_str();
		opts.mime_types = mimeTypes.c_str();
		// Without Accept-Encoding, so a stale "<file>.gz" next to the file is never served instead
		mg_http_message request{};
		if (http_msg) request = *http_msg;
		else request.method = mg_str("GET");
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

namespace {
	using TrafficClock = std::chrono::steady_clock;

	// Deferred requests' route and start, so their latency counts once the answer goes out
	struct DeferredTiming {
		std::string route;
		TrafficClock::time_point started;
		bool fromServer{};
		std::string address;
		uint64_t requestBytes{};
		uint32_t accountId{};
		std::string user;
		std::string apiKey; // the API key's name, when the request used one
	};
	std::unordered_map<unsigned long, DeferredTiming> g_DeferredTiming;

	// The bytes a reply puts on the wire besides its headers (a served file: its size)
	uint64_t ReplyBytes(const HTTPReply& reply) {
		if (!reply.file.empty() && reply.status == eHTTPStatusCode::OK) {
			std::error_code ec;
			const auto size = std::filesystem::file_size(reply.file, ec);
			return ec ? 0 : size;
		}
		return reply.message.size();
	}

	// A temporary file the reply sent: gone once mongoose has it open (on Windows, left for its maker to clear out)
	void RemoveSentFile(const HTTPReply& reply) {
		if (!reply.removeFile || reply.file.empty()) return;
		std::error_code ec;
		std::filesystem::remove(reply.file, ec);
	}

	// `accountId` and `user`: who the request was signed in as (0 and "" for none), for the Network page's web clients
	void CountRequest(const std::string& route, uint16_t status, TrafficClock::time_point started, uint64_t bytes, bool fromServer,
		const std::string& address, uint64_t requestBytes, uint32_t accountId = 0, const std::string& user = {}, const std::string& apiKeyName = {}) {
		const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(TrafficClock::now() - started).count();
		TrafficStats::Local().Http(TrafficStats::Now(), route, status, static_cast<uint64_t>(std::max<int64_t>(micros, 0)), bytes, fromServer);
		TrafficStats::Local().HttpClient(address, fromServer, requestBytes, bytes, accountId, user, apiKeyName);
	}
}

void HandleHTTPMessage(mg_connection* connection, const mg_http_message* http_msg) {
	if (g_HTTPRoutes.empty()) return;

	HTTPReply reply;
	const auto started = TrafficClock::now();
	// The route's pattern, not the path, so traffic diagnostics have one entry per route
	std::string trafficRoute = "(no route)";
	// Another server asking (the dashboard fetching from the UGC server), for the network diagram's links
	const bool fromServer = http_msg && mg_http_get_header(const_cast<mg_http_message*>(http_msg), TrafficStats::SERVER_HEADER) != nullptr;
	const auto clientAddress = GetClientIP(connection);
	const uint64_t requestBytes = http_msg ? http_msg->message.len : 0;
	// Who the request was signed in as, once the middleware has looked (the dashboard's session or API key)
	uint32_t signedInAccount = 0;
	std::string signedInUser, signedInKey; // signedInKey: the API key's name when the request used one
	
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
				if (level && Game::web.GetWSApiAccessCallback() && !Game::web.GetWSApiAccessCallback()(*level)) level.reset();
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
			
			CountRequest("GET /ws", level ? 101 : 401, started, 0, fromServer, clientAddress, requestBytes, level ? level->accountId : 0, {},
				level && level->apiKey ? level->apiKey->name : std::string());
			if (level) {
				mg_ws_upgrade(connection, const_cast<mg_http_message*>(http_msg), NULL);
				g_AuthenticatedWSConnections[connection] = { level->level, level->accountId, connectToken, apiToken,
					std::chrono::steady_clock::now() + WS_RECHECK_INTERVAL, level->apiKey, level->grants };
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
		auto routeItr = FindRoute(g_HTTPRoutes, method, uri);

		if (routeItr != g_HTTPRoutes.end()) {
			const auto& route = routeItr->second;
			trafficRoute = method_string + " " + routeItr->first.second;

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
			
			if (context.isAuthenticated) {
				signedInAccount = context.accountId;
				signedInUser = context.authenticatedUser;
				if (context.apiKey) signedInKey = context.apiKey->name;
			}

			// Call handler only if all middleware passed. A failing handler (e.g. a database error) answers 500
			// instead of taking the whole server down.
			if (chainPassed) {
				// Frame timing: a handler runs on the main thread, between ticks or inside one
				Profiler::Scope profile(Profiler::Intern(trafficRoute), Profiler::Phase::WEB);
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
	
	if (reply.deferred) {
		// Answered later (Web::Defer): the connection keeps is_resp set, so mongoose reads no further request on it
		const auto* cc = http_msg ? mg_http_get_header(const_cast<mg_http_message*>(http_msg), "Connection") : nullptr;
		g_Deferred.SetReplyOptions(connection->id, reply.headers, cc && mg_strcasecmp(*cc, mg_str("close")) == 0);
		// Requests the answers never came for (the client left) are forgotten now and then
		if (g_DeferredTiming.size() > 10000) g_DeferredTiming.clear();
		g_DeferredTiming[connection->id] = { std::move(trafficRoute), started, fromServer, clientAddress, requestBytes, signedInAccount, std::move(signedInUser), std::move(signedInKey) };
		return;
	}
	// The handler deferred and then failed: its late answer is dropped
	if (g_Deferred.IsPending(connection->id)) g_Deferred.Close(connection->id);

	SendReply(connection, reply, http_msg);
	CountRequest(trafficRoute, static_cast<uint16_t>(reply.status), started, ReplyBytes(reply), fromServer, clientAddress, requestBytes, signedInAccount, signedInUser, signedInKey);
	RemoveSentFile(reply);
}



namespace {
	// An upload whose body is arriving. Its connection's HTTP parsing is off (the body never sits in mongoose's
	// buffer), so the connection carries this one request and closes after its reply.
	struct Upload {
		const HTTPUploadRoute* route{};
		HTTPContext context;
		FILE* file{};
		uint64_t expected{};
		uint64_t received{};
		bool discard{};   // answered already (refused): what still arrives is dropped
		TrafficClock::time_point started;
		TrafficClock::time_point lastData;
		std::string trafficRoute;
	};
	std::unordered_map<unsigned long, Upload> g_Uploads;
	constexpr auto UPLOAD_IDLE_TIMEOUT = std::chrono::seconds(60);

	void AbortUpload(Upload& upload) {
		if (upload.file) {
			fclose(upload.file);
			upload.file = nullptr;
			std::error_code ec;
			std::filesystem::remove(upload.context.bodyFile, ec);
		}
	}

	// Answer an upload now and close its connection once the answer is out; what else arrives is dropped
	void ReplyAndClose(mg_connection* connection, Upload& upload, HTTPReply& reply) {
		AbortUpload(upload);
		upload.discard = true;
		reply.headers.push_back("Connection: close");
		SendReply(connection, reply, nullptr);
		CountRequest(upload.trafficRoute, static_cast<uint16_t>(reply.status), upload.started, ReplyBytes(reply), false, upload.context.clientIP,
			upload.received, upload.context.accountId, upload.context.authenticatedUser, upload.context.apiKey ? upload.context.apiKey->name : std::string());
		connection->is_draining = 1;
	}

	void JsonUploadError(HTTPReply& reply, eHTTPStatusCode status, const char* code, const char* message) {
		reply = HTTPReply{};
		reply.status = status;
		reply.contentType = eContentType::APPLICATION_JSON;
		reply.message = json{ {"success", false}, {"error", code}, {"message", message} }.dump();
	}

	void FinishUpload(mg_connection* connection, Upload& upload) {
		fflush(upload.file);
		const bool written = !ferror(upload.file);
		fclose(upload.file);
		upload.file = nullptr;
		upload.context.bodyBytes = upload.received;
		HTTPReply reply;
		if (!written) {
			std::error_code ec;
			std::filesystem::remove(upload.context.bodyFile, ec);
			JsonUploadError(reply, eHTTPStatusCode::INSUFFICIENT_STORAGE, "storage", "The upload could not be written");
			return ReplyAndClose(connection, upload, reply);
		}
		{
			Profiler::Scope profile(Profiler::Intern(upload.trafficRoute), Profiler::Phase::WEB);
			try {
				upload.route->handle(reply, upload.context);
			} catch (const std::exception& ex) {
				LOG("Error handling %s %s: %s", upload.context.method.c_str(), upload.context.path.c_str(), ex.what());
				if (g_Deferred.IsPending(connection->id)) g_Deferred.Close(connection->id);
				JsonUploadError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "internal", "Internal server error");
			}
		}
		upload.discard = true;
		if (reply.deferred) {
			g_Deferred.SetReplyOptions(connection->id, reply.headers, true);
			if (g_DeferredTiming.size() > 10000) g_DeferredTiming.clear();
			g_DeferredTiming[connection->id] = { upload.trafficRoute, upload.started, false, upload.context.clientIP, upload.received,
				upload.context.accountId, upload.context.authenticatedUser, upload.context.apiKey ? upload.context.apiKey->name : std::string() };
			return;
		}
		ReplyAndClose(connection, upload, reply);
	}

	// Write what has arrived of an upload's body to its file
	void ReadUpload(mg_connection* connection, Upload& upload, size_t skip) {
		const size_t available = connection->recv.len > skip ? connection->recv.len - skip : 0;
		if (upload.discard) {
			connection->recv.len = 0;
			return;
		}
		const auto take = static_cast<size_t>(std::min<uint64_t>(available, upload.expected - upload.received));
		if (take > 0 && fwrite(connection->recv.buf + skip, 1, take, upload.file) != take) {
			HTTPReply reply;
			JsonUploadError(reply, eHTTPStatusCode::INSUFFICIENT_STORAGE, "storage", "The upload could not be written");
			connection->recv.len = 0;
			return ReplyAndClose(connection, upload, reply);
		}
		upload.received += take;
		upload.lastData = TrafficClock::now();
		// Anything past the body is not ours to read: this connection carries one request
		connection->recv.len = 0;
		if (upload.received >= upload.expected) FinishUpload(connection, upload);
	}

	/**
	 * A request's headers are in: when it is for an upload route, take the request over from here (the middleware
	 * first, then its body straight to a file). Called again for the same request while an ordinary route's body is
	 * still arriving, so it does nothing unless an upload route matches.
	 */
	void HandleHTTPHeaders(mg_connection* connection, mg_http_message* http_msg) {
		if (g_UploadRoutes.empty() || !http_msg || g_Uploads.contains(connection->id)) return;
		const std::string methodString(http_msg->method.buf, http_msg->method.len);
		const eHTTPMethod method = magic_enum::enum_cast<eHTTPMethod>(methodString).value_or(eHTTPMethod::INVALID);
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		std::transform(uri.begin(), uri.end(), uri.begin(), ::tolower);
		const auto routeItr = FindRoute(g_UploadRoutes, method, uri);
		if (routeItr == g_UploadRoutes.end()) return;
		const auto& route = routeItr->second;

		auto& upload = g_Uploads[connection->id];
		upload.route = &route;
		upload.started = upload.lastData = TrafficClock::now();
		upload.trafficRoute = methodString + " " + routeItr->first.second;
		// The body is not in the message yet (and never will be): only the head is read here
		mg_http_message head = *http_msg;
		head.body.len = 0;
		PopulateHTTPContext(upload.context, &head, connection);
		const size_t headLength = http_msg->head.len;

		HTTPReply reply;
		const auto refuse = [&] {
			// Taking bytes out of the buffer is what tells mongoose the request is ours now
			connection->recv.len = 0;
			ReplyAndClose(connection, upload, reply);
		};

		std::vector<MiddlewarePtr> chain = g_GlobalMiddleware;
		chain.insert(chain.end(), route.middleware.begin(), route.middleware.end());
		for (const auto& middleware : chain) {
			bool passed = false;
			try {
				passed = middleware->Process(upload.context, reply);
			} catch (const std::exception& ex) {
				LOG("Error in middleware %s for %s: %s", middleware->GetName().c_str(), upload.context.path.c_str(), ex.what());
				JsonUploadError(reply, eHTTPStatusCode::INTERNAL_SERVER_ERROR, "internal", "Internal server error");
			}
			if (!passed) return refuse();
		}

		// The length has to be known before any of the body is taken
		const auto* length = mg_http_get_header(http_msg, "Content-Length");
		const auto parsed = length ? GeneralUtils::TryParse<uint64_t>(std::string_view(length->buf, length->len)) : std::nullopt;
		if (mg_http_get_header(http_msg, "Transfer-Encoding") || !parsed) {
			JsonUploadError(reply, eHTTPStatusCode::LENGTH_REQUIRED, "length_required", "Send the body with a Content-Length");
			return refuse();
		}
		if (*parsed > (route.maxBytes ? route.maxBytes() : 0)) {
			JsonUploadError(reply, eHTTPStatusCode::PAYLOAD_TOO_LARGE, "too_large", "The request is larger than this server takes");
			return refuse();
		}
		upload.expected = *parsed;
		upload.context.bodyFile = route.spoolFile ? route.spoolFile(upload.context) : std::string();
		if (!upload.context.bodyFile.empty()) upload.file = fopen(upload.context.bodyFile.c_str(), "wb");
		if (!upload.file) {
			JsonUploadError(reply, eHTTPStatusCode::SERVICE_UNAVAILABLE, "unavailable", "There is nowhere to keep the upload");
			return refuse();
		}

		if (const auto* expect = mg_http_get_header(http_msg, "Expect"); expect && mg_strcasecmp(*expect, mg_str("100-continue")) == 0) {
			static const char proceed[] = "HTTP/1.1 100 Continue\r\n\r\n";
			mg_send(connection, proceed, sizeof(proceed) - 1);
		}
		ReadUpload(connection, upload, headLength);
		// An empty buffer with an empty body would look untouched to mongoose; the head is always taken out above
	}

	void CloseIdleUploads(mg_connection* connection) {
		const auto it = g_Uploads.find(connection->id);
		if (it == g_Uploads.end() || it->second.discard) return;
		if (TrafficClock::now() - it->second.lastData < UPLOAD_IDLE_TIMEOUT) return;
		LOG_DEBUG("Closing an upload that stopped arriving");
		connection->is_closing = 1;
	}
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
			if (connItr == g_AuthenticatedWSConnections.end() || !MayReceive(connItr->second, index, g_WSSubscriptionLevels[index]())) {
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
		case MG_EV_HTTP_HDRS:
			HandleHTTPHeaders(connection, static_cast<mg_http_message*>(message_data));
			break;
		case MG_EV_READ:
			if (const auto upload = g_Uploads.find(connection->id); upload != g_Uploads.end()) ReadUpload(connection, upload->second, 0);
			break;
		case MG_EV_POLL:
			if (!g_Uploads.empty()) CloseIdleUploads(connection);
			break;
		case MG_EV_HTTP_MSG:
			HandleHTTPMessage(connection, static_cast<mg_http_message*>(message_data));
			break;
		case MG_EV_WS_MSG:
			HandleWSMessage(connection, static_cast<mg_ws_message*>(message_data));
			break;
		case MG_EV_CLOSE:
			g_AuthenticatedWSConnections.erase(connection);
			// A deferred answer that comes after this is dropped
			g_Deferred.Close(connection->id);
			if (const auto upload = g_Uploads.find(connection->id); upload != g_Uploads.end()) {
				AbortUpload(upload->second);
				g_Uploads.erase(upload);
			}
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

void Web::RegisterUploadRoute(HTTPUploadRoute route) {
	if (!Game::web.enabled) {
		LOG_DEBUG("Failed to register upload route %s: web server not enabled", route.path.c_str());
		return;
	}
	const auto path = route.path;
	const auto [_, success] = g_UploadRoutes.try_emplace({ route.method, path }, std::move(route));
	LOG_DEBUG("%s upload route %s", success ? "Registered" : "Failed to register", path.c_str());
}

size_t Web::PendingUploads() const {
	return g_Uploads.size();
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
	RegisterWSSubscription(subscription, std::move(minLevel), "");
}

void Web::RegisterWSSubscription(const std::string& subscription, std::function<uint8_t()> minLevel, std::string permission) {
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
		g_WSSubscriptionPermissions.push_back(std::move(permission));
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
	// Static destruction: the maps and queues the close events touch are gone by now, so the handlers are off
	// (HandleMessages checks enabled). Servers call Shutdown first, while they're still there.
	enabled = false;
	if (!managerFreed) mg_mgr_free(&mgr);
	managerFreed = true;
}

void Web::Shutdown() {
	if (managerFreed) return;
	// Closing the connections fires their close events, which still clean up (WebSocket clients, deferred requests)
	g_Deferred.CancelAll();
	mg_mgr_free(&mgr);
	managerFreed = true;
	enabled = false;
	g_AuthenticatedWSConnections.clear();
	for (auto& [_, upload] : g_Uploads) AbortUpload(upload);
	g_Uploads.clear();
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

	// Sent with the traffic reports (dServer)
	TrafficStats::Local().SetGauge("http_deferred_pending", [] { return static_cast<double>(g_Deferred.Pending()); });
	TrafficStats::Local().SetGauge("websocket_clients", [] { return static_cast<double>(g_AuthenticatedWSConnections.size()); });

	return true;
}

void Web::ReceiveRequests(int timeoutMs) {
	mg_mgr_poll(&mgr, timeoutMs);
	SendDeferredReplies();
	RecheckDueWebSockets();
}

void Web::SendDeferredReplies() {
	for (auto& finished : g_Deferred.Drain()) {
		mg_connection* connection = mgr.conns;
		while (connection && connection->id != finished.connection) connection = connection->next;
		if (!connection || connection->is_closing) { RemoveSentFile(finished.reply); continue; }
		// Clears is_resp once the reply is out, so mongoose reads the connection's next request again
		SendReply(connection, finished.reply, nullptr);
		if (const auto timing = g_DeferredTiming.find(finished.connection); timing != g_DeferredTiming.end()) {
			CountRequest(timing->second.route, static_cast<uint16_t>(finished.reply.status), timing->second.started, ReplyBytes(finished.reply), timing->second.fromServer,
				timing->second.address, timing->second.requestBytes, timing->second.accountId, timing->second.user, timing->second.apiKey);
			g_DeferredTiming.erase(timing);
		}
		RemoveSentFile(finished.reply);
		if (finished.close) connection->is_draining = 1;
	}
}

DeferredReply Web::Defer(HTTPReply& reply, const HTTPContext& context) {
	reply.deferred = g_Deferred.Begin(context.connectionId);
	return DeferredReply(reply.deferred);
}

size_t Web::PendingDeferred() const {
	return g_Deferred.Pending();
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
		if (connItr == g_AuthenticatedWSConnections.end() || connItr->second.accountId != accountId || !MayReceive(connItr->second, index, minLevel)) continue;
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
		if (connItr == g_AuthenticatedWSConnections.end() || !MayReceive(connItr->second, index, minLevel)) continue;
		mg_ws_send(wc, payload.c_str(), payload.size(), WEBSOCKET_OP_TEXT);
	}
}
