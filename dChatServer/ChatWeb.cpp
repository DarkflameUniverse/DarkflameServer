#include "ChatWeb.h"

#include "Logger.h"
#include "Game.h"
#include "json.hpp"
#include "dCommonVars.h"
#include "MessageType/Chat.h"
#include "dServer.h"
#include "dConfig.h"
#include "PlayerContainer.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"
#include "magic_enum.hpp"
#include "ChatPackets.h"
#include "StringifiedEnum.h"
#include "Database.h"
#include "ChatJSONUtils.h"
#include "JSONUtils.h"
#include "eGameMasterLevel.h"
#include "dChatFilter.h"
#include "TeamContainer.h"

using json = nlohmann::json;

void HandleHTTPPlayersRequest(HTTPReply& reply, std::string body) {
	const json data = Game::playerContainer;
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Players Online\"}" : data.dump();
}

void HandleHTTPTeamsRequest(HTTPReply& reply, std::string body) {
	const json data = TeamContainer::GetTeamContainer();
	reply.status = data.empty() ? eHTTPStatusCode::NO_CONTENT : eHTTPStatusCode::OK;
	reply.message = data.empty() ? "{\"error\":\"No Teams Online\"}" : data.dump();
}

void HandleHTTPAnnounceRequest(HTTPReply& reply, std::string body) {
	auto data = GeneralUtils::TryParse<json>(body);
	if (!data) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = "{\"error\":\"Invalid JSON\"}";
		return;
	}

	const auto& good_data = data.value();
	auto check = JSONUtils::CheckRequiredData(good_data, { "title", "message" });
	if (!check.empty()) {
		reply.status = eHTTPStatusCode::BAD_REQUEST;
		reply.message = check;
	} else {

		ChatPackets::Announcement announcement;
		announcement.title = good_data["title"];
		announcement.message = good_data["message"];
		announcement.Broadcast();

		reply.status = eHTTPStatusCode::OK;
		reply.message = "{\"status\":\"Announcement Sent\"}";
	}
}

void HandleWSChat(mg_connection* connection, json data) {
	auto check = JSONUtils::CheckRequiredData(data, { "user", "message", "gmlevel", "zone" });
	if (!check.empty()) {
		LOG_DEBUG("Received invalid websocket message: %s", check.c_str());
	} else {
		const auto user = data["user"].get<std::string>();
		const auto message = data["message"].get<std::string>();
		const auto gmlevel = GeneralUtils::TryParse<eGameMasterLevel>(data["gmlevel"].get<std::string>()).value_or(eGameMasterLevel::CIVILIAN);
		const auto zone = data["zone"].get<uint32_t>();

		const auto filter_check = Game::chatFilter->IsSentenceOkay(message, gmlevel);
		if (!filter_check.empty()) {
			LOG_DEBUG("Chat message \"%s\" from %s was not allowed", message.c_str(), user.c_str());
			data["error"] = "Chat message blocked by filter";
			data["filtered"] = json::array();
			for (const auto& [start, len] : filter_check) {
				data["filtered"].push_back(message.substr(start, len));
			}
			mg_ws_send(connection, data.dump().c_str(), data.dump().size(), WEBSOCKET_OP_TEXT);
			return;
		}
		LOG("%s: %s", user.c_str(), message.c_str());
		
		// TODO: Implement chat message handling from websocket message

	}
}

namespace ChatWeb {
	void RegisterRoutes() {
		
		// REST API v1 routes

		std::string v1_route = "/api/v1/";
		Game::web.RegisterHTTPRoute({
			.path = v1_route + "players",
			.method = eHTTPMethod::GET,
			.handle = HandleHTTPPlayersRequest
		});

		Game::web.RegisterHTTPRoute({
			.path = v1_route + "teams",
			.method = eHTTPMethod::GET,
			.handle = HandleHTTPTeamsRequest
		});

		Game::web.RegisterHTTPRoute({
			.path = v1_route + "announce",
			.method = eHTTPMethod::POST,
			.handle = HandleHTTPAnnounceRequest
		});

		// WebSocket Events Handlers

		// Game::web.RegisterWSEvent({
		// 	.name = "chat",
		// 	.handle = HandleWSChat
		// });

		// WebSocket subscriptions

		Game::web.RegisterWSSubscription("player");
	}

	void SendWSPlayerUpdate(const PlayerData& player, eActivityType activityType) {
		json data;
		data["player_data"] = player;
		data["update_type"] = magic_enum::enum_name(activityType);
		Game::web.SendWSMessage("player", data);
	}
}

