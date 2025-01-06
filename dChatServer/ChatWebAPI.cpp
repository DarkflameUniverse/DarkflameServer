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
#include "JSONUtils.h"
#include "eHTTPMethod.h"
#include "eHTTPStatusCode.h"
#include "magic_enum.hpp"
#include "ChatPackets.h"

#include "StringifiedEnum.h"

using json = nlohmann::json;

typedef struct mg_connection mg_connection;
typedef struct mg_http_message mg_http_message;

namespace {
	const std::string root_path = "/api/v1/";
	const char* json_content_type = "Content-Type: application/json\r\n";
}

struct HTTPReply {
	eHTTPStatusCode status = eHTTPStatusCode::NOT_FOUND;
	std::string message = "{\"error\":\"Not Found\"}";
};

bool CheckValidJSON(std::optional<json> data, HTTPReply& reply) {
	if (!data) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid JSON\"}";
		return false;
	}
	return true;
}

void HandlePlayersRequest(HTTPReply& reply) {
	const json data = Game::playerContainer;
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Players Online\"}" : data.dump();
}

void HandleTeamsRequest(HTTPReply& reply) {
	const json data = Game::playerContainer.GetTeamContainer();
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Teams Online\"}" : data.dump();
}

void HandleInvalidRoute(HTTPReply& reply) {
	reply.status = eHTTPStatusCode::NOT_FOUND;
	reply.message = "{\"error\":\"Invalid Route\"}";
}

void HandleGET(HTTPReply& reply, const ChatWebAPI::eRoute& route , const std::string& body) {
	switch (route) {
		case ChatWebAPI::eRoute::PLAYERS:
			HandlePlayersRequest(reply);
			break;
		case ChatWebAPI::eRoute::TEAMS:
			HandleTeamsRequest(reply);
			break;
		case ChatWebAPI::eRoute::INVALID:
		default:
			HandleInvalidRoute(reply);
			break;
	}
}

void HandleAnnounceRequest(HTTPReply& reply, const std::optional<json>& data) {
	if (!CheckValidJSON(data, reply)) return;

	const auto& good_data = data.value();
	auto check = JSONUtils::CheckRequiredData(good_data, { "title", "message" });
	if (!check.empty()) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = check;
	} else {

		ChatPackets::Announcement announcement;
		announcement.title = good_data["title"];
		announcement.message = good_data["message"];
		announcement.Send();

		reply.status = eHTTPStatusCode::OK;
		reply.message = "{\"status\":\"Announcement Sent\"}";
	}
}

void HandlePOST(HTTPReply& reply, const ChatWebAPI::eRoute& route , const std::string& body) {
	auto data = GeneralUtils::TryParse<json>(body);
	switch (route) {
		case ChatWebAPI::eRoute::ANNOUNCE:{
			HandleAnnounceRequest(reply, data.value());
			break;
		}
		case ChatWebAPI::eRoute::INVALID:
		default:
			HandleInvalidRoute(reply);
			break;
	}
}

void HandleHTTPMessage(mg_connection* connection, const mg_http_message* http_msg) {
	HTTPReply reply;
	
	if (!http_msg) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid Request\"}";
	} else {
		
		// convert method from cstring to std string
		std::string method_string(http_msg->method.buf, http_msg->method.len);
		// get mehtod from mg to enum
		const eHTTPMethod method = magic_enum::enum_cast<eHTTPMethod>(method_string).value_or(eHTTPMethod::INVALID);

		// convert uri from cstring to std string
		std::string uri(http_msg->uri.buf, http_msg->uri.len);
		// check for root path
		if (uri.find(root_path) == 0) {
			// remove root path from uri
			uri.erase(0, root_path.length());
			// convert uri to uppercase
			std::transform(uri.begin(), uri.end(), uri.begin(), ::toupper);
			// convert uri string to route enum
			ChatWebAPI::eRoute route = magic_enum::enum_cast<ChatWebAPI::eRoute>(uri).value_or(ChatWebAPI::eRoute::INVALID);

			// convert body from cstring to std string
			std::string body(http_msg->body.buf, http_msg->body.len);

			switch (method) {
			case eHTTPMethod::GET:
				HandleGET(reply, route, body);
				break;
			case eHTTPMethod::POST:
				HandlePOST(reply, route, body);
				break;
			case eHTTPMethod::PUT:	
			case eHTTPMethod::DELETE:
			case eHTTPMethod::HEAD:	
			case eHTTPMethod::CONNECT:
			case eHTTPMethod::OPTIONS:
			case eHTTPMethod::TRACE:
			case eHTTPMethod::PATCH:
			case eHTTPMethod::INVALID:
			default:
				reply.status = eHTTPStatusCode::METHOD_NOT_ALLOWED;
				reply.message = "{\"error\":\"Invalid Method\"}";
				break;
			}
		}
	}
	mg_http_reply(connection, static_cast<int>(reply.status), json_content_type, reply.message.c_str());
}


void HandleRequests(mg_connection* connection, int request, void* request_data) {
	switch (request) {
		case MG_EV_HTTP_MSG:
			HandleHTTPMessage(connection, static_cast<mg_http_message*>(request_data));
			break;
		default:
			break;
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
