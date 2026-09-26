#ifndef __WEB_H__
#define __WEB_H__

#include <functional>
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include "mongoose.h"
#include "json_fwd.hpp"
#include "eHTTPStatusCode.h"
#include "HTTPContext.h"
#include "IHTTPMiddleware.h"

// Forward declarations for game namespace
// so that we can access the data anywhere
class Web;
namespace Game {
	extern Web web;
}

enum class eHTTPMethod;

// Forward declaration for mongoose manager
typedef struct mg_mgr mg_mgr;

// Content type enum for HTTP responses
enum class eContentType {
	APPLICATION_JSON,
	TEXT_HTML,
	TEXT_CSS,
	TEXT_JAVASCRIPT,
	TEXT_PLAIN,
	TEXT_CSV,
	IMAGE_PNG,
	IMAGE_JPEG,
	APPLICATION_OCTET_STREAM,
	TEXT_PROMETHEUS // the Prometheus text exposition format
};

// For passing HTTP messages between functions
struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
	eContentType contentType = eContentType::APPLICATION_JSON;
	std::string location = "";  // For redirect responses (Location header)
	std::vector<std::string> headers{}; // Extra raw headers, e.g. "Set-Cookie: a=b"
	// When set on a 200 reply, this file is streamed from disk as the body (with contentType and headers) instead of
	// message, so large downloads never sit in memory
	std::string file{};
};

// HTTP route structure
// This structure is used to register HTTP routes
// with the server. Each route has a path, method, optional middleware,
// and a handler function that will be called when the route is matched.
struct HTTPRoute {
	std::string path;
	eHTTPMethod method;
	std::vector<MiddlewarePtr> middleware;
	std::function<void(HTTPReply&, const HTTPContext&)> handle;
};

// WebSocket event structure
// This structure is used to register WebSocket events
// with the server. Each event has a name and a handler function
// that will be called when the event is triggered.
struct WSEvent {
	std::string name;
	std::function<void(mg_connection*, nlohmann::json)> handle;
};

// Subscription status for WebSocket clients
enum SubscriptionStatus {
	UNSUBSCRIBED = 0,
	SUBSCRIBED = 1
};

// Who a WebSocket connection belongs to
struct WSAuth {
	uint8_t level{};
	uint32_t accountId{};
};

// WebSocket authentication callback function type
// Returns the permission level and account of the token's owner, or nullopt if the token is invalid.
// It is called again for open connections (every minute, and on RecheckWebSockets), so revoked sessions, bans and
// demotions reach live sockets too.
using WSAuthCallback = std::function<std::optional<WSAuth>(const std::string&)>;

class Web {
public:
	// Constructor
	Web();
	// Destructor
	~Web();
	// Handle incoming messages
	// Handle pending HTTP and WebSocket traffic, waiting up to timeoutMs for some to arrive
	void ReceiveRequests(int timeoutMs = 15);
	// Start the web server
	// Returns true if the server started successfully
	bool Startup(const std::string& listen_ip, const uint32_t listen_port);
	// Register HTTP route to be handled by the server
	void RegisterHTTPRoute(HTTPRoute route);
	// Register WebSocket event to be handled by the server
	void RegisterWSEvent(WSEvent event);
	// Register WebSocket subscription to be handled by the server.
	// Only connections whose permission level is at least minLevel may subscribe or receive it.
	void RegisterWSSubscription(const std::string& subscription, uint8_t minLevel = 0);
	// The level is looked up each time (for permissions that can change while running)
	void RegisterWSSubscription(const std::string& subscription, std::function<uint8_t()> minLevel);
	// Add global middleware that applies to all routes
	void AddGlobalMiddleware(MiddlewarePtr middleware);
	// Set WebSocket authentication callback for token validation
	void SetWSAuthCallback(WSAuthCallback callback) { wsAuthCallback = callback; }
	// Whether a GM level may connect with an API token (Authorization: Bearer) rather than the browser's cookie
	void SetWSApiAccessCallback(std::function<bool(uint8_t)> callback) { wsApiAccessCallback = std::move(callback); }
	// Returns if the web server is enabled
	bool IsEnabled() const { return enabled; };
	// Send a message to all connected WebSocket clients that are subscribed to the given topic
	void static SendWSMessage(std::string sub, nlohmann::json& message);
	// Send a message on a topic only to the subscribed connections of one account
	void static SendWSMessageToAccount(std::string sub, nlohmann::json& message, uint32_t accountId);
	// Check the token of open WebSocket connections again on the next poll (one account's, or all when 0).
	// Connections that no longer verify are closed; the others get their current level.
	void RecheckWebSockets(uint32_t accountId = 0);
	// Security headers added to every HTTP response
	void SetDefaultHeaders(std::vector<std::string> headers) { defaultHeaders = std::move(headers); }
	const std::vector<std::string>& GetDefaultHeaders() const { return defaultHeaders; }
	// Get mongoose manager for direct access
	mg_mgr& GetManager() { return mgr; };
	// Get WebSocket auth callback (used during WebSocket upgrade)
	WSAuthCallback GetWSAuthCallback() const { return wsAuthCallback; }
	const std::function<bool(uint8_t)>& GetWSApiAccessCallback() const { return wsApiAccessCallback; }
private:
	// mongoose manager
	mg_mgr mgr;
	// If the web server is enabled
	bool enabled = false;
	// WebSocket authentication callback
	WSAuthCallback wsAuthCallback = nullptr;
	std::function<bool(uint8_t)> wsApiAccessCallback = nullptr;
	std::vector<std::string> defaultHeaders{};
};

#endif // !__WEB_H__
