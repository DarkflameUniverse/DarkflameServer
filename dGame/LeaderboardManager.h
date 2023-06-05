#ifndef __LEADERBOARDMANAGER__H__
#define __LEADERBOARDMANAGER__H__

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "Singleton.h"
#include "dCommonVars.h"
#include "LDFFormat.h"

namespace sql {
	class ResultSet;
};

namespace RakNet {
	class BitStream;
};

class Score {
public:
	Score() {
		primaryScore = 0;
		secondaryScore = 0;
		tertiaryScore = 0;
	}
	Score(const float primaryScore, const float secondaryScore = 0, const float tertiaryScore = 0) {
		this->primaryScore = primaryScore;
		this->secondaryScore = secondaryScore;
		this->tertiaryScore = tertiaryScore;
	}
	bool operator<(const Score& rhs) const {
		return primaryScore < rhs.primaryScore || (primaryScore == rhs.primaryScore && secondaryScore < rhs.secondaryScore) || (primaryScore == rhs.primaryScore && secondaryScore == rhs.secondaryScore && tertiaryScore < rhs.tertiaryScore);
	}
	bool operator>(const Score& rhs) const {
		return primaryScore > rhs.primaryScore || (primaryScore == rhs.primaryScore && secondaryScore > rhs.secondaryScore) || (primaryScore == rhs.primaryScore && secondaryScore == rhs.secondaryScore && tertiaryScore > rhs.tertiaryScore);
	}
	void SetPrimaryScore(const float score) { primaryScore = score; }
	float GetPrimaryScore() const { return primaryScore; }

	void SetSecondaryScore(const float score) { secondaryScore = score; }
	float GetSecondaryScore() const { return secondaryScore; }

	void SetTertiaryScore(const float score) { tertiaryScore = score; }
	float GetTertiaryScore() const { return tertiaryScore; }
private:
	float primaryScore;
	float secondaryScore;
	float tertiaryScore;
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
	Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const Leaderboard::Type = None);

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
	void Serialize(RakNet::BitStream* bitStream) const;

	/**
	 * Builds the leaderboard from the database based on the associated gameID
	 *
	 * @param resultStart The index to start the leaderboard at. Zero indexed.
	 * @param resultEnd The index to end the leaderboard at. Zero indexed.
	 */
	void SetupLeaderboard(uint32_t resultStart = 0, uint32_t resultEnd = 10);

	/**
	 * Sends the leaderboard to the client specified by targetID.
	 */
	void Send(const LWOOBJID targetID) const;

	// Helper function to get the columns, ordering and insert format for a leaderboard
	static const std::string_view GetOrdering(Type leaderboardType);
private:
	// Takes the resulting query from a leaderboard lookup and converts it to the LDF we need
	// to send it to a client.
	void QueryToLdf(std::unique_ptr<sql::ResultSet>& rows);

	using LeaderboardEntry = std::vector<LDFBaseData*>;
	using LeaderboardEntries = std::vector<LeaderboardEntry>;

	LeaderboardEntries entries;
	LWOOBJID relatedPlayer;
	GameID gameID;
	InfoType infoType;
	Leaderboard::Type leaderboardType;
	bool weekly;
};

namespace LeaderboardManager {
	void SendLeaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const LWOOBJID playerID, const LWOOBJID targetID, const uint32_t resultStart = 0, const uint32_t resultEnd = 10);

	void SaveScore(const LWOOBJID& playerID, const GameID activityId, const float primaryScore, const float secondaryScore = 0, const float tertiaryScore = 0);

	Leaderboard::Type GetLeaderboardType(const GameID gameID);
	extern std::map<GameID, Leaderboard::Type> leaderboardCache;
};

#endif  //!__LEADERBOARDMANAGER__H__
