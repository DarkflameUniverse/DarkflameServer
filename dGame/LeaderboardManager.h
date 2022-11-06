#pragma once
#include <vector>
#include <climits>
#include "dCommonVars.h"

struct LeaderboardEntry {
	uint64_t playerID;
	std::string playerName;
	uint32_t time;
	uint32_t score;
	uint32_t placement;
	time_t lastPlayed;
};

enum InfoType : uint32_t {
	Top,     // Top 11 all time players
	Standings, // Ranking of the current player
	Friends    // Ranking between friends
};

enum LeaderboardType : uint32_t {
	ShootingGallery,
	Racing,
	MonumentRace,
	FootRace,
	Survival = 5,
	SurvivalNS = 6,
	None = UINT_MAX
};

class Leaderboard {
public:
	Leaderboard(uint32_t gameID, uint32_t infoType, bool weekly, std::vector<LeaderboardEntry> entries,
		LWOOBJID relatedPlayer = LWOOBJID_EMPTY, LeaderboardType = None);
	std::vector<LeaderboardEntry> GetEntries();
	[[nodiscard]] std::u16string ToString() const;
	[[nodiscard]] uint32_t GetGameID() const;
	[[nodiscard]] uint32_t GetInfoType() const;
	void Send(LWOOBJID targetID) const;
private:
	std::vector<LeaderboardEntry> entries{};
	LWOOBJID relatedPlayer;
	uint32_t gameID;
	uint32_t infoType;
	LeaderboardType leaderboardType;
	bool weekly;
};

class LeaderboardManager {
public:
	static LeaderboardManager* Instance() {
		if (address == nullptr)
			address = new LeaderboardManager;
		return address;
	}
	static void SendLeaderboard(uint32_t gameID, InfoType infoType, bool weekly, LWOOBJID targetID,
		LWOOBJID playerID = LWOOBJID_EMPTY);
	static Leaderboard* GetLeaderboard(uint32_t gameID, InfoType infoType, bool weekly, LWOOBJID playerID = LWOOBJID_EMPTY);
	static void SaveScore(LWOOBJID playerID, uint32_t gameID, uint32_t score, uint32_t time);
	static LeaderboardType GetLeaderboardType(uint32_t gameID);
private:
	static LeaderboardManager* address;

	// Modified 12/12/2021: Existing queries were renamed to be more descriptive.
	static const std::string topPlayersScoreQuery;
	static const std::string friendsScoreQuery;
	static const std::string standingsScoreQuery;
	static const std::string topPlayersScoreQueryAsc;
	static const std::string friendsScoreQueryAsc;
	static const std::string standingsScoreQueryAsc;

	// Added 12/12/2021: Queries dictated by time are needed for certain minigames.
	static const std::string topPlayersTimeQuery;
	static const std::string friendsTimeQuery;
	static const std::string standingsTimeQuery;
	static const std::string topPlayersTimeQueryAsc;
	static const std::string friendsTimeQueryAsc;
	static const std::string standingsTimeQueryAsc;
};

