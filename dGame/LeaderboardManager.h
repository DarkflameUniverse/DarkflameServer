#ifndef __LEADERBOARDMANAGER__H__
#define __LEADERBOARDMANAGER__H__

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "dCommonVars.h"
#include "LDFFormat.h"

namespace RakNet {
	class BitStream;
};

using GameID = uint32_t;

class Leaderboard {
public:

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
		UnusedLeaderboard4, // There is no 4 defined anywhere in the cdclient, but it takes a Score.
		Survival,
		SurvivalNS,
		Donations,
		None
	};
	Leaderboard() = delete;
	Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const uint32_t numResults, const Leaderboard::Type = None);

	~Leaderboard();

	/**
	 * @brief Resets the leaderboard state and frees its allocated memory
	 *
	 */
	void Clear();

	/**
	 * Serialize the Leaderboard to a BitStream
	 *
	 * Expensive!  Leaderboards are very string intensive so be wary of performatnce calling this method.
	 */
	void Serialize(RakNet::BitStream& bitStream) const;

	/**
	 * Builds the leaderboard from the database based on the associated gameID
	 *
	 * @param resultStart The index to start the leaderboard at. Zero indexed.
	 * @param resultEnd The index to end the leaderboard at. Zero indexed.
	 */
	void SetupLeaderboard(bool weekly);

	/**
	 * Sends the leaderboard to the client specified by targetID.
	 */
	void Send(const LWOOBJID targetID) const;

	

private:
	using LeaderboardEntry = std::vector<LDFBaseData*>;
	using LeaderboardEntries = std::vector<LeaderboardEntry>;

	LeaderboardEntries entries;
	LWOOBJID relatedPlayer;
	GameID gameID;
	InfoType infoType;
	Leaderboard::Type leaderboardType;
	bool weekly;
	uint32_t numResults;
public:
	LeaderboardEntry& PushBackEntry() {
		return entries.emplace_back();
	}

	Type GetLeaderboardType() const {
		return leaderboardType;
	}
};

namespace LeaderboardManager {
	void SendLeaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const LWOOBJID playerID, const LWOOBJID targetID, const uint32_t numResults);

	void SaveScore(const LWOOBJID& playerID, const GameID activityId, const float primaryScore, const float secondaryScore = 0, const float tertiaryScore = 0);

	Leaderboard::Type GetLeaderboardType(const GameID gameID);
	extern std::map<GameID, Leaderboard::Type> leaderboardCache;
};

#endif  //!__LEADERBOARDMANAGER__H__
