#include "ChatWebAPI.h"

#include "Logger.h"
#include "Game.h"
#include "json.hpp"
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "dServer.h"
#include "dConfig.h"
#include "PlayerContainer.h"

void ChatWebAPI::HandleRequests(struct mg_connection *c, int ev, void *ev_data) {
	if (ev == MG_EV_HTTP_MSG) {  // New HTTP request received
		struct mg_http_message *hm = static_cast<struct mg_http_message *>(ev_data);  // Parsed HTTP request
		if (!hm) {
			mg_http_reply(c, 400, json_content_type, "{\"error\":\"Invalid Request\"}");
			return;
		}

		// Handle Post requests
		if (mg_strcmp(hm->method, mg_str("POST")) == 0) {
			// handle announcements
			if (mg_match(hm->uri, mg_str((root_path + "announce").c_str()), NULL)) { 
				auto data = ParseJSON(hm->body.buf);
				if (!data) {
					mg_http_reply(c, 400, json_content_type, "{\"error\":\"Invalid JSON\"}");
					return;
				}

				if (!data.value().contains("title")) {
					mg_http_reply(c, 400, json_content_type, "{\"error\":\"Missing paramater: title\"}");
					return;
				}
				std::string title = data.value()["title"];
				if (!data.value().contains("message")) {
					mg_http_reply(c, 400, json_content_type, "{\"error\":\"Missing paramater: message\"}");
					return;
				}
				std::string message = data.value()["message"];
				LOG_DEBUG("Announcement: %s - %s", title.c_str(), message.c_str());
				// build and send the packet to all world servers
				CBITSTREAM;
				BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, MessageType::Chat::GM_ANNOUNCE);
				bitStream.Write<uint32_t>(title.size());
				bitStream.Write(title);
				bitStream.Write<uint32_t>(message.size());
				bitStream.Write(message);
				Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
				mg_http_reply(c, 200, json_content_type, "{\"status\":\"Announcement Sent\"}");
			} else {
				// 404 Not Found
				mg_http_reply(c, 404, json_content_type, "{\"error\":\"Not Found\"}");
			}
		// Handle GET Requests
		} else if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
			// Get All Online players
			if (mg_match(hm->uri, mg_str((root_path + "players").c_str()), NULL)) {
				auto data = json::array();
				for (auto& [playerID, playerData ]: Game::playerContainer.GetAllPlayers()){
					if (!playerData) continue;
					data.push_back(playerData.to_json());
				}
				if (data.empty()) {
					mg_http_reply(c, 200, json_content_type, "{\"error\":\"No Players Online\"}");
				} else {
					mg_http_reply(c, 200, json_content_type, data.dump().c_str());
				}
			} else if (mg_match(hm->uri, mg_str((root_path + "teams").c_str()), NULL)) {
				// Get Teams
				auto data = json::array();
				for (auto& teamData: Game::playerContainer.GetAllTeams()){
					if (!teamData) continue;
					json toInsert;
					toInsert["id"] = teamData->teamID;
					toInsert["loot_flag"] = teamData->lootFlag;
					toInsert["local"] = teamData->local;

					auto& leader = Game::playerContainer.GetPlayerData(teamData->leaderID);
					toInsert["leader"] = leader.to_json();

					json members;
					for (auto& member : teamData->memberIDs){
						auto& playerData = Game::playerContainer.GetPlayerData(member);
			
						if (!playerData) continue;
						members.push_back(playerData.to_json());
					}
					toInsert["members"] = members;
					data.push_back(toInsert);
				}

				if (data.empty()) {
					mg_http_reply(c, 200, json_content_type, "{\"error\":\"No Teams Online\"}");
				} else {
					mg_http_reply(c, 200, json_content_type, data.dump().c_str());
				}
			} else {
				// 404 Not Found
				mg_http_reply(c, 404, json_content_type, "{\"error\":\"Not Found\"}");
			}
		} else {
			// 404 Not Found
			mg_http_reply(c, 404, json_content_type, "{\"error\":\"Not Found\"}");
		}
	}
}


ChatWebAPI::ChatWebAPI() {
	if (Game::logger->GetLogDebugStatements()) mg_log_set(MG_LL_DEBUG);
	// make listen address
	std::string listen_ip = Game::config->GetValue("web_server_listen_ip");
	std::string listen_port = Game::config->GetValue("wed_server_listen_port");
	LOG_DEBUG("Starting web server on %s:%s", listen_ip.c_str(), listen_port.c_str());
	std::string listen_address = "http://" + listen_ip + ":" + listen_port;
	LOG_DEBUG("Starting web server on %s", listen_address.c_str());
	mg_mgr_init(&mgr);  // Initialise event manager
	mg_http_listen(&mgr, listen_address.c_str(), HandleRequests, NULL);  // Create HTTP listener
}

ChatWebAPI::~ChatWebAPI() {
	mg_mgr_free(&mgr);
}

void ChatWebAPI::ReceiveRequests() {
	mg_mgr_poll(&mgr, 15);
}

// TODO: Move to GeneralUtils
std::optional<json> ChatWebAPI::ParseJSON(char * data) {
	try {
		return std::make_optional<json>(json::parse(data));
	} catch (const std::exception& e) {
		LOG_DEBUG("Failed to parse JSON: %s", e.what());
		return std::nullopt;
	}
}


