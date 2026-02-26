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
	IMAGE_PNG,
	IMAGE_JPEG,
	APPLICATION_OCTET_STREAM
};

// For passing HTTP messages between functions
struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
	eContentType contentType = eContentType::APPLICATION_JSON;
	std::string location = "";  // For redirect responses (Location header)
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

class Web {
public:
	// Constructor
	Web();
	// Destructor
	~Web();
	// Handle incoming messages
	void ReceiveRequests();
	// Start the web server
	// Returns true if the server started successfully
	bool Startup(const std::string& listen_ip, const uint32_t listen_port);
	// Register HTTP route to be handled by the server
	void RegisterHTTPRoute(HTTPRoute route);
	// Register WebSocket event to be handled by the server
	void RegisterWSEvent(WSEvent event);
	// Register WebSocket subscription to be handled by the server
	void RegisterWSSubscription(const std::string& subscription);
	// Add global middleware that applies to all routes
	void AddGlobalMiddleware(MiddlewarePtr middleware);
	// Returns if the web server is enabled
	bool IsEnabled() const { return enabled; };
	// Send a message to all connected WebSocket clients that are subscribed to the given topic
	void static SendWSMessage(std::string sub, nlohmann::json& message);
	// Get mongoose manager for direct access
	mg_mgr& GetManager() { return mgr; };
private:
	// mongoose manager
	mg_mgr mgr;
	// If the web server is enabled
	bool enabled = false;
};

#endif // !__WEB_H__
