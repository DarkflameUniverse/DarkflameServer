#pragma once
#include <map>
#include "dCommonVars.h"
#include "RakString.h"
#include <vector>
#include "Game.h"
#include "dServer.h"
#include <unordered_map>

struct IgnoreData {
	inline bool operator==(const std::string& other) const noexcept {
		return playerName == other;
	}

	inline bool operator==(const LWOOBJID& other) const noexcept {
		return playerId == other;
	}

	LWOOBJID playerId;
	std::string playerName;
};

struct PlayerData {
	LWOOBJID playerID;
	std::string playerName;
	SystemAddress sysAddr;
	LWOZONEID zoneID;
	std::vector<FriendData> friends;
	std::vector<IgnoreData> ignoredPlayers;
	time_t muteExpire;
	uint8_t countOfBestFriends = 0;
};

struct TeamData {
	TeamData();
	LWOOBJID teamID = LWOOBJID_EMPTY; // Internal use
	LWOOBJID leaderID = LWOOBJID_EMPTY;
	std::vector<LWOOBJID> memberIDs{};
	uint8_t lootFlag = 0;
	bool local = false;
	LWOZONEID zoneId = {};
};

class PlayerContainer {
public:
	~PlayerContainer();

	void Initialize();
	void InsertPlayer(Packet* packet);
	void RemovePlayer(Packet* packet);
	void MuteUpdate(Packet* packet);
	void CreateTeamServer(Packet* packet);
	void BroadcastMuteUpdate(LWOOBJID player, time_t time);

	PlayerData* GetPlayerData(const LWOOBJID& playerID) {
		auto it = m_Players.find(playerID);
		if (it != m_Players.end()) return it->second;
		return nullptr;
	}

	PlayerData* GetPlayerData(const std::string& playerName) {
		for (auto player : m_Players) {
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
	uint32_t GetMaxNumberOfBestFriends() { return m_MaxNumberOfBestFriends; }
	uint32_t GetMaxNumberOfFriends() { return m_MaxNumberOfFriends; }

	std::map<LWOOBJID, PlayerData*>& GetAllPlayerData() { return m_Players; }

private:
	LWOOBJID m_TeamIDCounter = 0;
	std::map<LWOOBJID, PlayerData*> m_Players;
	std::vector<TeamData*> mTeams;
	std::unordered_map<LWOOBJID, std::u16string> m_Names;
	uint32_t m_MaxNumberOfBestFriends = 5;
	uint32_t m_MaxNumberOfFriends = 50;
};

