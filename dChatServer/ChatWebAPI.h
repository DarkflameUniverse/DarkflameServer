#ifndef CHATWEBAPI_H
#define CHATWEBAPI_H

#include "mongoose.h"
#include "json_fwd.hpp"
#include "GeneralUtils.h"

using json = nlohmann::json;

typedef struct mg_mgr mg_mgr;

class ChatWebAPI {
public:
	ChatWebAPI();
	~ChatWebAPI();
	void ReceiveRequests();
	void Listen();
private:
	mg_mgr mgr;
};

#endif
