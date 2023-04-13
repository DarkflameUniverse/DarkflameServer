#pragma once
#include <climits>
#include <map>
#include <vector>

#include "Singleton.h"
#include "dCommonVars.h"

namespace RakNet{
	class BitStream;
};

typedef uint32_t GameID;

class Leaderboard {
public:
	struct Entry {
		LWOOBJID playerID;
		uint32_t time;
		uint32_t score;
		uint32_t placement;
		time_t lastPlayed;
		std::string playerName;
	};
	typedef std::vector<Entry> LeaderboardEntries;

	// Enums for leaderboards
	enum InfoType : uint32_t {
		Top,     // Top 11 all time players
		MyStanding, // Ranking of the current player
		Friends    // Ranking between friends
	};

	enum Type : uint32_t {
		ShootingGallery,
		Racing,
		MonumentRace,
		FootRace,
		Survival = 5,
		SurvivalNS = 6,
		None = UINT_MAX
	};

	Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const Leaderboard::Type = None);

	/**
	 * Serialize the Leaderboard to a BitStream
	 * 
	 * Expensive!  Leaderboards are very string intensive so be wary of performatnce calling this method.
	 */
	void Serialize(RakNet::BitStream* bitStream) const;

	/**
	 * Based on the associated gameID, return true if the score provided
	 * is better than the current entries' score
	 * @param score 
	 * @return true 
	 * @return false 
	 */
	bool IsScoreBetter(const uint32_t score) const { return false; };

	/**
	 * Builds the leaderboard from the database based on the associated gameID
	 */
	void SetupLeaderboard();

	/**
	 * Sends the leaderboard to the client specified by targetID.
	 */
	void Send(LWOOBJID targetID) const;

	/**
	 * Adds a new entry to the leaderboard
	 * Used for debug only!
	 */
	void AddEntry(Entry entry) { entries.push_back(entry); }
private:
	LeaderboardEntries entries;
	LWOOBJID relatedPlayer;
	GameID gameID;
	InfoType infoType;
	Leaderboard::Type leaderboardType;
	bool weekly;
};

class LeaderboardManager: public Singleton<LeaderboardManager> {
	typedef std::map<GameID, Leaderboard::Type> LeaderboardCache;
public:
	void SendLeaderboard(GameID gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID targetID,
		LWOOBJID playerID = LWOOBJID_EMPTY);
	void SaveScore(LWOOBJID playerID, GameID gameID, uint32_t score, uint32_t time);
private:
	Leaderboard::Type GetLeaderboardType(const GameID gameID);
	void GetLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID = LWOOBJID_EMPTY);
	LeaderboardCache leaderboardCache;
};

