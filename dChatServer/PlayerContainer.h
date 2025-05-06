#pragma once
#include <map>
#include "dCommonVars.h"
#include "RakString.h"
#include <vector>
#include "Game.h"
#include "dServer.h"
#include <unordered_map>

enum class eGameMasterLevel : uint8_t;

struct TeamData;

struct IgnoreData {
	IgnoreData(const std::string& name, const LWOOBJID& id) : playerName{ name }, playerId{ id } {}
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
	operator bool() const noexcept {
		return playerID != LWOOBJID_EMPTY;
	}

	bool operator==(const PlayerData& other) const noexcept {
		return playerID == other.playerID;
	}

	bool GetIsMuted() const {
		return muteExpire == 1 || muteExpire > time(NULL);
	}

	SystemAddress worldServerSysAddr{};
	LWOZONEID zoneID{};
	LWOOBJID playerID = LWOOBJID_EMPTY;
	time_t muteExpire = 0;
	uint8_t countOfBestFriends = 0;
	std::string playerName;
	std::vector<FriendData> friends;
	std::vector<IgnoreData> ignoredPlayers;
	eGameMasterLevel gmLevel = static_cast<eGameMasterLevel>(0); // CIVILLIAN
	bool isFTP = false;
	bool isLogin = false;
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
	void Initialize();
	void InsertPlayer(Packet* packet);
	void ScheduleRemovePlayer(Packet* packet);
	void RemovePlayer(const LWOOBJID playerID);
	void MuteUpdate(Packet* packet);
	void BroadcastMuteUpdate(LWOOBJID player, time_t time);
	void Shutdown();

	const PlayerData& GetPlayerData(const LWOOBJID& playerID);
	const PlayerData& GetPlayerData(const std::string& playerName);
	PlayerData& GetPlayerDataMutable(const LWOOBJID& playerID);
	PlayerData& GetPlayerDataMutable(const std::string& playerName);
	std::u16string GetName(LWOOBJID playerID);
	LWOOBJID GetId(const std::u16string& playerName);
	void Update(const float deltaTime);

	uint32_t GetPlayerCount() { return m_PlayerCount; };
	uint32_t GetSimCount() { return m_SimCount; };
	const std::map<LWOOBJID, PlayerData>& GetAllPlayers() const { return m_Players; };
	uint32_t GetMaxNumberOfBestFriends() { return m_MaxNumberOfBestFriends; }
	uint32_t GetMaxNumberOfFriends() { return m_MaxNumberOfFriends; }
	bool PlayerBeingRemoved(const LWOOBJID playerID) { return m_PlayersToRemove.contains(playerID); }

private:
	std::map<LWOOBJID, PlayerData> m_Players;
	std::unordered_map<LWOOBJID, std::u16string> m_Names;
	std::map<LWOOBJID, float> m_PlayersToRemove;
	uint32_t m_MaxNumberOfBestFriends = 5;
	uint32_t m_MaxNumberOfFriends = 50;
	uint32_t m_PlayerCount = 0;
	uint32_t m_SimCount = 0;
};

