#ifndef __WEB_H__
#define __WEB_H__

#include <functional>
#include <string>
#include <optional>
#include "mongoose.h"
#include "json_fwd.hpp"
#include "eHTTPStatusCode.h"

class Web;
namespace Game {
	extern Web web;
}

enum class eHTTPMethod;

typedef struct mg_mgr mg_mgr;

struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
};

struct HTTPRoute {
	std::string path;
	eHTTPMethod method;
	std::function<void(HTTPReply&, const std::string&)> handle;
};

struct WSEvent {
	std::string name;
	std::function<void(mg_connection*, nlohmann::json)> handle;
};

struct WSMessage {
	uint32_t id;
	std::string sub;
	std::string message;
};

class Web {
public:
	Web();
	~Web();
	void ReceiveRequests();
	void static SendWSMessage(std::string sub, nlohmann::json& message);
	bool Startup(const std::string& listen_ip, const uint32_t listen_port);
	void RegisterHTTPRoute(HTTPRoute route);
	void RegisterWSEvent(WSEvent event);
	void RegisterWSSubscription(const std::string& subscription);
	bool IsEnabled() const { return enabled; };
private:
	mg_mgr mgr;
	bool enabled = false;
};

#endif // !__WEB_H__
