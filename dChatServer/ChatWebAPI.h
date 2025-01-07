#ifndef CHATWEBAPI_H
#define CHATWEBAPI_H

#include "mongoose.h"

typedef struct mg_mgr mg_mgr;

class ChatWebAPI {
public:
	ChatWebAPI();
	~ChatWebAPI();
	void ReceiveRequests();
	void Listen();
	enum class eRoute {
		// GET
		PLAYERS,
		TEAMS,
		// POST
		ANNOUNCE,
		CSR,
		// INVALID
		INVALID
	};
private:
	mg_mgr mgr;


};

#endif
