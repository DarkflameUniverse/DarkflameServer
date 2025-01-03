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

typedef struct mg_connection mg_connection;
typedef struct mg_http_message mg_http_message;

namespace {
	const std::string root_path = "/api/v1/";
	const char* json_content_type = "Content-Type: application/json\r\n";
}

void HandleRequests(mg_connection* connection, int request, void* request_data) {
	if (request == MG_EV_HTTP_MSG) {
		const mg_http_message* const http_msg = static_cast<mg_http_message*>(request_data);
		if (!http_msg) {
			mg_http_reply(connection, 400, json_content_type, "{\"error\":\"Invalid Request\"}");
			return;
		}

		// Handle Post requests
		if (mg_strcmp(http_msg->method, mg_str("POST")) == 0) {
			// handle announcements
			if (mg_match(http_msg->uri, mg_str((root_path + "announce").c_str()), NULL)) {
				auto data = GeneralUtils::TryParse<json>(http_msg->body.buf);
				if (!data) {
					mg_http_reply(connection, 400, json_content_type, "{\"error\":\"Invalid JSON\"}");
					return;
				}

				if (!data.value().contains("title")) {
					mg_http_reply(connection, 400, json_content_type, "{\"error\":\"Missing paramater: title\"}");
					return;
				}
				std::string title = data.value()["title"];
				if (!data.value().contains("message")) {
					mg_http_reply(connection, 400, json_content_type, "{\"error\":\"Missing paramater: message\"}");
					return;
				}
				std::string message = data.value()["message"];

				// build and send the packet to all world servers
				{
					CBITSTREAM;
					BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GM_ANNOUNCE);
					bitStream.Write<uint32_t>(title.size());
					bitStream.Write(title);
					bitStream.Write<uint32_t>(message.size());
					bitStream.Write(message);
					Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
				}

				mg_http_reply(connection, 200, json_content_type, "{\"status\":\"Announcement Sent\"}");
				return;
			}
			// Handle GET Requests
		} else if (mg_strcmp(http_msg->method, mg_str("GET")) == 0) {

			// Get All Online players
			if (mg_match(http_msg->uri, mg_str((root_path + "players").c_str()), NULL)) {
				auto data = json::array();
				for (auto& [playerID, playerData] : Game::playerContainer.GetAllPlayers()) {
					if (!playerData) continue;
					data.push_back(playerData.to_json());
				}
				if (data.empty()) {
					mg_http_reply(connection, 200, json_content_type, "{\"error\":\"No Players Online\"}");
				} else {
					mg_http_reply(connection, 200, json_content_type, data.dump().c_str());
				}
				return;

			} else if (mg_match(http_msg->uri, mg_str((root_path + "teams").c_str()), NULL)) {

				// Get Teams
				auto data = json::array();
				for (auto& teamData : Game::playerContainer.GetAllTeams()) {
					if (!teamData) continue;
					json toInsert;
					toInsert["id"] = teamData->teamID;
					toInsert["loot_flag"] = teamData->lootFlag;
					toInsert["local"] = teamData->local;

					auto& leader = Game::playerContainer.GetPlayerData(teamData->leaderID);
					toInsert["leader"] = leader.to_json();

					json members;
					for (auto& member : teamData->memberIDs) {
						auto& playerData = Game::playerContainer.GetPlayerData(member);

						if (!playerData) continue;
						members.push_back(playerData.to_json());
					}
					toInsert["members"] = members;
					data.push_back(toInsert);
				}

				if (data.empty()) {
					mg_http_reply(connection, 200, json_content_type, "{\"error\":\"No Teams Online\"}");
				} else {
					mg_http_reply(connection, 200, json_content_type, data.dump().c_str());
				}
				return;

			}
		}

		// If it hasn't been handled then reply 404 Not Found
		mg_http_reply(connection, 404, json_content_type, "{\"error\":\"Not Found\"}");
	}
}


ChatWebAPI::ChatWebAPI() {
	if (Game::logger->GetLogDebugStatements()) mg_log_set(MG_LL_DEBUG);
	mg_mgr_init(&mgr);  // Initialize event manager
}

ChatWebAPI::~ChatWebAPI() {
	mg_mgr_free(&mgr);
}

void ChatWebAPI::Listen() {
	// make listen address
	const std::string& listen_ip = Game::config->GetValue("web_server_listen_ip");
	const std::string& listen_port = Game::config->GetValue("wed_server_listen_port");
	const std::string& listen_address = "http://" + listen_ip + ":" + listen_port;
	LOG("Starting web server on %s", listen_address.c_str());

	mg_http_listen(&mgr, listen_address.c_str(), HandleRequests, NULL);  // Create HTTP listener
}

void ChatWebAPI::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}
