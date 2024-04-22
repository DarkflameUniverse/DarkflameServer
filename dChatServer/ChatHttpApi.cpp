#include <cstdint>
#include <nlohmann/json.hpp>

#include "ChatHttpApi.h"
#include "dCommonVars.h"
#include "eConnectionType.h"
#include "eChatMessageType.h"
#include "httplib.h"
#include "dServer.h"
#include "PlayerContainer.h"

using json = nlohmann::json;

namespace {
	httplib::Server m_APIServer;
}

void ChatHttpApi::Listen(const uint32_t port) {
	m_APIServer.Post("/announce", [](const httplib::Request& req, httplib::Response& res) {
		const json data = json::parse(req.body);
		if (!data.contains("title")) {
			res.set_content("{\"error\":\"Missing paramater: title\"}", "application/json");
			res.status = 400;
			return;
		}
		std::string title = data["title"];
		if (!data.contains("message")) {
			res.set_content("{\"error\":\"Missing paramater: message\"}", "application/json");
			res.status = 400;
			return;
		}
		std::string message = data["message"];
		// build and send the packet to all world servers
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, eConnectionType::CHAT, eChatMessageType::GM_ANNOUNCE);
		bitStream.Write<uint32_t>(title.size());
		bitStream.Write(title);
		bitStream.Write<uint32_t>(message.size());
		bitStream.Write(message);
		Game::server->Send(bitStream, UNASSIGNED_SYSTEM_ADDRESS, true);
	});

	m_APIServer.Get("/players", [](const httplib::Request& req, httplib::Response& res) {
		auto data = json::array();
		for (auto& [playerID, playerData ]: Game::playerContainer.GetAllPlayers()){
			if (!playerData) continue;
			data.push_back(playerData.to_json());
		}
		res.set_content(data.dump(), "application/json");
		if (data.empty()) res.status = 204;
	});

	m_APIServer.Get("/teams", [](const httplib::Request& req, httplib::Response& res) {
		auto data = json::array();
		for (auto& teamData: Game::playerContainer.GetAllTeams()){
			if (!teamData) continue;
			json toInsert;
			toInsert["id"] = teamData->teamID;
			toInsert["loot_flag"] = teamData->lootFlag;
			toInsert["local"] = teamData->local;

			auto leader = Game::playerContainer.GetPlayerData(teamData->leaderID);
			toInsert["leader"] = leader.to_json();

			json members;
			for (auto& member : teamData->memberIDs){
				auto playerData = Game::playerContainer.GetPlayerData(member);
				if (!playerData) continue;
				members.push_back(playerData.to_json());
			}
			toInsert["members"] = members;
			data.push_back(toInsert);
		}
		res.set_content(data.dump(), "application/json");
		if (data.empty()) res.status = 204;
	});

	m_APIServer.listen("0.0.0.0", port);
};

void ChatHttpApi::Stop(){
	m_APIServer.stop();
}