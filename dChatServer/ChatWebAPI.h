#ifndef INCLUDED_CHATWEBAPI
#define INCLUDED_CHATWEBAPI

#include "mongoose.h"
#include "json_fwd.hpp"
#include "GeneralUtils.h"

using json = nlohmann::json;

class ChatWebAPI {
public:
	ChatWebAPI();
	~ChatWebAPI();
	void ReceiveRequests();
	void Listen();
private:
	struct mg_mgr mgr;
	static void HandleRequests(struct mg_connection *c, int ev, void *ev_data);
	inline static const std::string root_path = "/api/v1/";
	inline static const char * json_content_type = "Content-Type: application/json\r\n";
	static std::optional<json> ParseJSON(char * data);
};

#endif