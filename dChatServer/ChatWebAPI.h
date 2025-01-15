#ifndef __CHATWEBAPI_H__
#define __CHATWEBAPI_H__
#include <string>
#include <functional>

#include "mongoose.h"
#include "eHTTPStatusCode.h"

enum class eHTTPMethod;

typedef struct mg_mgr mg_mgr;

struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
};

struct WebAPIHTTPRoute {
	std::string path;
	eHTTPMethod method;
	std::function<void(HTTPReply&, const std::string&)> handle;
};

class ChatWebAPI {
public:
	ChatWebAPI();
	~ChatWebAPI();
	void ReceiveRequests();
	void RegisterHTTPRoutes(WebAPIHTTPRoute route);
	bool Startup();
private:
	mg_mgr mgr;

};

#endif // __CHATWEBAPI_H__
