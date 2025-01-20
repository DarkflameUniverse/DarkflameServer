#ifndef __CHATWEBAPI_H__
#define __CHATWEBAPI_H__
#include <string>
#include <functional>

#include "mongoose.h"
#include "json_fwd.hpp"
#include "eHTTPStatusCode.h"

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

struct WSAction {
	std::string action;
	std::function<void(nlohmann::json)> handle;
};

class ChatWebAPI {
public:
	ChatWebAPI();
	~ChatWebAPI();
	void ReceiveRequests();
	void RegisterHTTPRoute(HTTPRoute route);
	void RegisterWSAction(WSAction action);
	void SendWSMessage(const std::string& message);
	bool Startup();
private:
	mg_mgr mgr;
};

#endif // __CHATWEBAPI_H__
