#include "JSONUtils.h"

#include "json.hpp"

using json = nlohmann::json;

void to_json(json& data, const PlayerData& playerData) {
	data["id"] = playerData.playerID;
	data["name"] = playerData.playerName;
	data["gm_level"] = playerData.gmLevel;
	data["muted"] = playerData.GetIsMuted();

	auto& zoneID = data["zone_id"];
	zoneID["map_id"] = playerData.zoneID.GetMapID();
	zoneID["instance_id"] = playerData.zoneID.GetInstanceID();
	zoneID["clone_id"] = playerData.zoneID.GetCloneID();
}

void to_json(json& data, const PlayerContainer& playerContainer) {
	data = json::array();
	for(auto& playerData : playerContainer.GetAllPlayers()) {
		if (playerData.first == LWOOBJID_EMPTY) continue;
		data.push_back(playerData.second);
	}
}

void to_json(json& data, const TeamContainer& teamContainer) {
	for (auto& teamData : Game::playerContainer.GetTeams()) {
		if (!teamData) continue;
		data.push_back(*teamData);
	}
}

void to_json(json& data, const TeamData& teamData) {
	data["id"] = teamData.teamID;
	data["loot_flag"] = teamData.lootFlag;
	data["local"] = teamData.local;

	auto& leader = Game::playerContainer.GetPlayerData(teamData.leaderID);
	data["leader"] = leader.playerName;

	auto& members = data["members"];
	for (auto& member : teamData.memberIDs) {
		auto& playerData = Game::playerContainer.GetPlayerData(member);

		if (!playerData) continue;
		members.push_back(playerData);
	}
}

std::string JSONUtils::CheckRequiredData(const json& data, const std::vector<std::string>& requiredData) {
	json check;
	check["error"] = json::array();
	for (const auto& required : requiredData) {
		if (!data.contains(required)) {
			check["error"].push_back("Missing Parameter: " + required);
		} else if (data[required] == "") {
			check["error"].push_back("Empty Parameter: " + required);
		}
	}
	return check["error"].empty() ? "" : check.dump();
}
