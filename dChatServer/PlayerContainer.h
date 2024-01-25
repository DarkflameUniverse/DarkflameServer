#pragma once
#include <map>
#include "dCommonVars.h"
#include "RakString.h"
#include <vector>
#include "Game.h"
#include "dServer.h"
#include <unordered_map>

enum class eGameMasterLevel : uint8_t;

struct IgnoreData {
	IgnoreData(const std::string& name, const LWOOBJID& id) : playerName(name), playerId(id) {}
	inline bool operator==(const std::string& other) const noexcept {
		return playerName == other;
	}

	inline bool operator==(const LWOOBJID& other) const noexcept {
		return playerId == other;
	}

	LWOOBJID playerId = LWOOBJID_EMPTY;
	std::string playerName;
};

struct PlayerData {
	PlayerData();
	operator bool() const noexcept {
		return playerID != LWOOBJID_EMPTY;
	}

	bool operator==(const PlayerData& other) const noexcept {
		return playerID == other.playerID;
	}

	bool GetIsMuted() const {
		return muteExpire == 1 || muteExpire > time(NULL);
	}

	SystemAddress sysAddr{};
	LWOZONEID zoneID{};
	LWOOBJID playerID = LWOOBJID_EMPTY;
	time_t muteExpire = 0;
	uint8_t countOfBestFriends = 0;
	std::string playerName;
	std::vector<FriendData> friends;
	std::vector<IgnoreData> ignoredPlayers;
	eGameMasterLevel gmLevel;
	bool isFTP = false;
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

	const PlayerData& GetPlayerData(const LWOOBJID& playerID);
	const PlayerData& GetPlayerData(const std::string& playerName);
	PlayerData& GetPlayerDataMutable(const LWOOBJID& playerID);
	PlayerData& GetPlayerDataMutable(const std::string& playerName);

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
	uint32_t GetMaxNumberOfBestFriends() { return m_MaxNumberOfBestFriends; }
	uint32_t GetMaxNumberOfFriends() { return m_MaxNumberOfFriends; }

private:
	LWOOBJID m_TeamIDCounter = 0;
	std::map<LWOOBJID, PlayerData> m_Players;
	std::vector<TeamData*> mTeams;
	std::unordered_map<LWOOBJID, std::u16string> m_Names;
	uint32_t m_MaxNumberOfBestFriends = 5;
	uint32_t m_MaxNumberOfFriends = 50;
};

