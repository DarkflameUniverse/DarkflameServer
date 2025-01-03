#include "ChatWebAPI.h"

#include "Logger.h"
#include "Game.h"
#include "json.hpp"
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "dServer.h"
#include "dConfig.h"
#include "PlayerContainer.h"
#include "GeneralUtils.h"

#include "json.hpp"

using json = nlohmann::json;

typedef struct mg_connection mg_connection;
typedef struct mg_http_message mg_http_message;

namespace {
	const std::string root_path = "/api/v1/";
	const char* json_content_type = "Content-Type: application/json\r\n";
}

struct HttpReply {
	uint32_t status = 404;
	std::string message = "{\"error\":\"Not Found\"}";
};

void HandleRequests(mg_connection* connection, int request, void* request_data) {
	if (request == MG_EV_HTTP_MSG) {
		HttpReply reply;
		const mg_http_message* const http_msg = static_cast<mg_http_message*>(request_data);
		if (!http_msg) {
			reply.status = 400;
			reply.message = "{\"error\":\"Invalid Request\"}";
		} else {
			// Handle Post requests
			if (mg_strcmp(http_msg->method, mg_str("POST")) == 0) {
				auto data = GeneralUtils::TryParse<json>(http_msg->body.buf);
				if (!data) {
					reply.status = 400;
					reply.message = "{\"error\":\"Invalid JSON\"}";
				} else if (mg_match(http_msg->uri, mg_str((root_path + "announce").c_str()), NULL)) {
					auto& jsonBuffer = data.value();
					// handle announcements

					if (!jsonBuffer.contains("title")) {
						reply.status = 400;
						reply.message = "{\"error\":\"Missing paramater: title\"}";
					} else if (!jsonBuffer.contains("message")) {
						reply.status = 400;
						reply.message = "{\"error\":\"Missing paramater: message\"}";
					} else {
						std::string title = jsonBuffer["title"];
						std::string message = jsonBuffer["message"];

						// build and send the packet to all world servers
						CBITSTREAM;
						BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GM_ANNOUNCE);
						bitStream.Write<uint32_t>(title.size());
						bitStream.Write(title);
						bitStream.Write<uint32_t>(message.size());
						bitStream.Write(message);
						SEND_PACKET_BROADCAST;

						reply.status = 200;
						reply.message = "{\"status\":\"Announcement Sent\"}";
					}
				}
				// Handle GET Requests
			} else if (mg_strcmp(http_msg->method, mg_str("GET")) == 0) {
				// Get All Online players
				if (mg_match(http_msg->uri, mg_str((root_path + "players").c_str()), NULL)) {
					const json data = Game::playerContainer;

					reply.status = 200;
					reply.message = data.empty() ? "{\"error\":\"No Players Online\"}" : data.dump();
				} else if (mg_match(http_msg->uri, mg_str((root_path + "teams").c_str()), NULL)) {
					// Get Teams
					const json data = Game::playerContainer.GetTeamComtainer();

					reply.status = 200;
					reply.message = data.empty() ? "{\"error\":\"No Teams Online\"}" : data.dump();
				}
			}
		}

		LOG_DEBUG("Replying with status %d: %s", reply.status, reply.message.c_str());
		mg_http_reply(connection, reply.status, json_content_type, reply.message.c_str());
	}
}


ChatWebAPI::ChatWebAPI() {
	mg_log_set(MG_LL_NONE);
	mg_mgr_init(&mgr);  // Initialize event manager
}

ChatWebAPI::~ChatWebAPI() {
	mg_mgr_free(&mgr);
}

void ChatWebAPI::Listen() {
	// make listen address
	std::string listen_ip = Game::config->GetValue("web_server_listen_ip");
	if (listen_ip == "localhost") listen_ip = "127.0.0.1";

	const std::string& listen_port = Game::config->GetValue("web_server_listen_port");
	const std::string& listen_address = "http://" + listen_ip + ":" + listen_port;
	LOG("Starting web server on %s", listen_address.c_str());

	// Create HTTP listener
	if (!mg_http_listen(&mgr, listen_address.c_str(), HandleRequests, NULL)) {
		LOG("Failed to create web server listener");
	}
}

void ChatWebAPI::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}
