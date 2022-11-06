#pragma once
#include <map>
#include "dCommonVars.h"
#include "RakString.h"
#include <vector>
#include "Game.h"
#include "dServer.h"
#include <unordered_map>

struct PlayerData {
	LWOOBJID playerID;
	std::string playerName;
	SystemAddress sysAddr;
	LWOZONEID zoneID;
	std::vector<FriendData> friends;
	time_t muteExpire;
	uint8_t countOfBestFriends = 0;
};

struct TeamData {
	LWOOBJID teamID = LWOOBJID_EMPTY; // Internal use
	LWOOBJID leaderID = LWOOBJID_EMPTY;
	std::vector<LWOOBJID> memberIDs{};
	uint8_t lootFlag = 0;
	bool local = false;
	LWOZONEID zoneId = {};
};

class PlayerContainer {
public:
	PlayerContainer();
	~PlayerContainer();

	void InsertPlayer(Packet* packet);
	void RemovePlayer(Packet* packet);
	void MuteUpdate(Packet* packet);
	void CreateTeamServer(Packet* packet);
	void BroadcastMuteUpdate(LWOOBJID player, time_t time);

	PlayerData* GetPlayerData(const LWOOBJID& playerID) {
		auto it = mPlayers.find(playerID);
		if (it != mPlayers.end()) return it->second;
		return nullptr;
	}

	PlayerData* GetPlayerData(const std::string& playerName) {
		for (auto player : mPlayers) {
			if (player.second) {
				std::string pn = player.second->playerName.c_str();
				if (pn == playerName) return player.second;
			}
		}

		return nullptr;
	}

	TeamData* CreateLocalTeam(std::vector<LWOOBJID> members);
	TeamData* CreateTeam(LWOOBJID leader, bool local = false);
	TeamData* GetTeam(LWOOBJID playerID);
	void AddMember(TeamData* team, LWOOBJID playerID);
	void RemoveMember(TeamData* team, LWOOBJID playerID, bool disband, bool kicked, bool leaving, bool silent = false);
	void PromoteMember(TeamData* team, LWOOBJID newLeader);
	void DisbandTeam(TeamData* team);
	void TeamStatusUpdate(TeamData* team);
	void UpdateTeamsOnWorld(TeamData* team, bool deleteTeam);
	std::u16string GetName(LWOOBJID playerID);
	LWOOBJID GetId(const std::u16string& playerName);
	bool GetIsMuted(PlayerData* data);

	std::map<LWOOBJID, PlayerData*>& GetAllPlayerData() { return mPlayers; }

private:
	LWOOBJID mTeamIDCounter = 0;
	std::map<LWOOBJID, PlayerData*> mPlayers;
	std::vector<TeamData*> mTeams;
	std::unordered_map<LWOOBJID, std::u16string> mNames;
};

