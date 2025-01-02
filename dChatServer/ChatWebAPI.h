#ifndef INCLUDED_CHATWEBAPI_LINUX
#define INCLUDED_CHATWEBAPI_LINUX

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string>

#include "json_fwd.hpp"
using json = nlohmann::json;

class ChatWebAPI {
public:
	ChatWebAPI(std::string ip_address, int port);
	~ChatWebAPI();
	void HandleRequest();

private:
	std::string m_ip_address;
	int m_port;
	int m_socket;
	int m_new_socket;
	long m_incomingMessage;
	struct sockaddr_in m_socketAddress;
	unsigned int m_socketAddress_len;
	std::string m_serverMessage;
	struct timeval m_tv;
	fd_set fdread;

	std::string buildResponse(int status, std::string input);
	void sendResponse(int status, std::string input);
	void HandleAction(json data);
};

#endif