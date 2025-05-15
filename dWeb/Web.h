#ifndef __WEB_H__
#define __WEB_H__

#include <functional>
#include <string>
#include <optional>
#include "mongoose.h"
#include "json_fwd.hpp"
#include "eHTTPStatusCode.h"

// Forward declarations for game namespace
// so that we can access the data anywhere
class Web;
namespace Game {
	extern Web web;
}

enum class eHTTPMethod;

// Forward declaration for mongoose manager
typedef struct mg_mgr mg_mgr;

// For passing HTTP messages between functions
struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
};

// HTTP route structure
// This structure is used to register HTTP routes
// with the server. Each route has a path, method, and a handler function
// that will be called when the route is matched.
struct HTTPRoute {
	std::string path;
	eHTTPMethod method;
	std::function<void(HTTPReply&, const std::string&)> handle;
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
	// Returns if the web server is enabled
	bool IsEnabled() const { return enabled; };
	// Send a message to all connected WebSocket clients that are subscribed to the given topic
	void static SendWSMessage(std::string sub, nlohmann::json& message);
private:
	// mongoose manager
	mg_mgr mgr;
	// If the web server is enabled
	bool enabled = false;
};

#endif // !__WEB_H__
