
#include <iostream>
#include <sstream>
#include <unistd.h>


#include "ChatWebAPI.h"
#include "Logger.h"
#include "Game.h"
#include "json.hpp"
#include "picohttpparser.h"

namespace {
	const int BUFFER_SIZE = 30720;
}

ChatWebAPI::ChatWebAPI(std::string ip_address, int port) :
						m_ip_address(ip_address), 
						m_port(port),
						m_socket(),
						m_new_socket(),
						m_incomingMessage(),
						m_socketAddress(),
						m_socketAddress_len(sizeof(m_socketAddress)) {

	m_socketAddress.sin_family = AF_INET; // IPv4
	m_socketAddress.sin_port = htons(m_port);
	m_socketAddress.sin_addr.s_addr = inet_addr(m_ip_address.c_str());

	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_socket < 0) {
		LOG_DEBUG("Cannot create socket");
		return;
	}
	if (bind(m_socket, reinterpret_cast<sockaddr*> (&m_socketAddress), m_socketAddress_len) < 0) {
		LOG_DEBUG("Cannot connect socket to address");
		return;
	}
	if (listen(m_socket, 20) < 0) {
		LOG_DEBUG("Socket listen failed");
		return;
	}
	return;
}

ChatWebAPI::~ChatWebAPI() {
	close(m_socket);
	close(m_new_socket);
}

void ChatWebAPI::HandleRequest() {
	m_tv.tv_sec = 0;
	m_tv.tv_usec = 33;

	FD_ZERO(&fdread);
	FD_SET(m_socket, &fdread);
	int32_t selectStatus = select(m_socket + 1, &fdread, NULL, NULL, &m_tv);

	switch (selectStatus) {
		case -1:
			// error
			LOG_DEBUG("Error in select");
			break;
		case 0:
			// timeout, I.E. nothing to read
			// LOG_DEBUG("Timeout in select");
			break;

		default: { // available to read 

			m_new_socket = accept(m_socket, reinterpret_cast<sockaddr*> (&m_socketAddress), &m_socketAddress_len);
			if (m_new_socket < 0) {
				LOG_DEBUG("Failed to accept connection");
			}

			int bytesReceived;

			char buffer[BUFFER_SIZE] = { 0 };
			bytesReceived = read(m_new_socket, buffer, BUFFER_SIZE);
			if (bytesReceived < 0) {
				LOG_DEBUG("Failed to read bytes from client socket connection");
			} else {
				LOG_DEBUG("------ Received message from client ------");
			}
			int status = 404;
			std::string response = "{\"error\": \"not found\"}";

			const char *method_c {};
			const char *path_c {};
			size_t buflen = 0, method_len, path_len, num_headers;
			std::array<struct phr_header, 100> headers {};
			int pret, minor_version;

			num_headers = sizeof(headers) / sizeof(headers[0]);
			
			pret = phr_parse_request(buffer, strlen(buffer), &method_c, &method_len, &path_c, &path_len, &minor_version, headers.data(), &num_headers, 0);

			// cleanup
			std::string method(method_c, method_len);
			std::string path(path_c, path_len);
			std::map<std::string, std::string> headerMap;
			for (size_t i = 0; i != num_headers; ++i) {
				std::string headerName(headers[i].name, headers[i].name_len);
				std::string headerValue(headers[i].value, headers[i].value_len);
				headerMap[headerName] = headerValue;
			}
			// log headers
			for (auto& header : headerMap) {
				LOG_DEBUG("header: %s: %s", header.first.c_str(), header.second.c_str());
			}

			std::string content (buffer + pret);
			LOG_DEBUG("content: %s", content.c_str());

			if (headerMap["Content-Type"] == "application/json") {
				auto data = json::parse(content);
				HandleAction(data);
			}			
			break;
		}
	}
	close(m_new_socket);
}

std::string ChatWebAPI::buildResponse(int status, std::string input) {
	std::ostringstream ss;
	ss << "HTTP/1.1 " << status <<  " OK\nContent-Type: application/json\nContent-Length: " << input.size() << "\n\n" << input;

	return ss.str();
}

void ChatWebAPI::sendResponse(int status, std::string input) {
	long bytesSent;
	std::string response = buildResponse(status, input);
	bytesSent = write(m_new_socket, response.c_str(), response.size());
	if (bytesSent == response.size()) {
		LOG_DEBUG("------ Server Response sent to client ------");
	} else {
		LOG_DEBUG("Error sending response to client");
	}
}

void ChatWebAPI::HandleAction(json data) {

}

