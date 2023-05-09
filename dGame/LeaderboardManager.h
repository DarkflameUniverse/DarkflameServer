#pragma once
#include <climits>
#include <map>
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

typedef uint32_t GameID;

class Leaderboard {
public:
	using LeaderboardEntry = std::vector<LDFBaseData*>;
	using LeaderboardEntries = std::vector<LeaderboardEntry>;

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
		None = UINT_MAX
	};

	Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const Leaderboard::Type = None);

	~Leaderboard();

	/**
	 * Serialize the Leaderboard to a BitStream
	 *
	 * Expensive!  Leaderboards are very string intensive so be wary of performatnce calling this method.
	 */
	void Serialize(RakNet::BitStream* bitStream);

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
	 * 
	 * @param resultStart The index to start the leaderboard at. Zero indexed.
	 * @param resultEnd The index to end the leaderboard at. Zero indexed.
	 */
	void SetupLeaderboard(uint32_t resultStart = 0, uint32_t resultEnd = 10);

	/**
	 * Sends the leaderboard to the client specified by targetID.
	 */
	void Send(LWOOBJID targetID);

	// Helper functions to get the columns, ordering and insert format for a leaderboard
	static std::string GetColumns(Type leaderboardType);
	static std::string GetInsertFormat(Type leaderboardType);
	static std::string GetOrdering(Type leaderboardType);
private:
	inline void WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, LDFBaseData* data);

	// Returns true if the string needs formatting
	bool GetRankingQuery(std::string& lookupReturn) const;

	// Takes the resulting query from a leaderboard lookup and converts it to the LDF we need
	// to send it to a client.
	void QueryToLdf(std::unique_ptr<sql::ResultSet>& rows);

	LeaderboardEntries entries;
	LWOOBJID relatedPlayer;
	GameID gameID;
	InfoType infoType;
	Leaderboard::Type leaderboardType;
	bool weekly;
};

class LeaderboardManager : public Singleton<LeaderboardManager> {
	typedef std::map<GameID, Leaderboard::Type> LeaderboardCache;
public:
	void SendLeaderboard(GameID gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID, uint32_t resultStart = 0, uint32_t resultEnd = 10);

	// Saves a score to the database for the Racing minigame
	inline void SaveRacingScore(const LWOOBJID& playerID, GameID gameID, uint32_t bestLapTime, uint32_t bestTime, bool won) {
		SaveScore(playerID, gameID, Leaderboard::Racing, 3, bestLapTime, bestTime, won);
	};

	// Saves a score to the database for the Shooting Gallery minigame
	inline void SaveShootingGalleryScore(const LWOOBJID& playerID, GameID gameID, uint32_t score, float hitPercentage, uint32_t streak) {
		SaveScore(playerID, gameID, Leaderboard::ShootingGallery, 3, score, hitPercentage, streak);
	};

	// Saves a score to the database for the footrace minigame
	inline void SaveFootRaceScore(const LWOOBJID& playerID, GameID gameID, uint32_t bestTime) {
		SaveScore(playerID, gameID, Leaderboard::FootRace, 1, bestTime);
	};

	// Saves a score to the database for the Monument footrace minigame
	inline void SaveMonumentRaceScore(const LWOOBJID& playerID, GameID gameID, uint32_t bestTime) {
		SaveScore(playerID, gameID, Leaderboard::MonumentRace, 1, bestTime);
	};

	// Saves a score to the database for the Survival minigame
	inline void SaveSurvivalScore(const LWOOBJID& playerID, GameID gameID, uint32_t score, uint32_t waves) {
		SaveScore(playerID, gameID, Leaderboard::Survival, 2, score, waves);
	};

	// Saves a score to the database for the SurvivalNS minigame
	// Same as the Survival minigame, but good for explicitness
	inline void SaveSurvivalNSScore(const LWOOBJID& playerID, GameID gameID, uint32_t score, uint32_t waves) {
		SaveScore(playerID, gameID, Leaderboard::SurvivalNS, 2, score, waves);
	};

	// Saves a score to the database for the Donations minigame
	inline void SaveDonationsScore(const LWOOBJID& playerID, GameID gameID, uint32_t score) {
		SaveScore(playerID, gameID, Leaderboard::Donations, 1, score);
	};

private:
	void SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, va_list args);
	void GetLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID = LWOOBJID_EMPTY);

	/**
	 * @brief Public facing Score saving method.  This method is simply a wrapper to ensure va_end is called properly.
	 *
	 * @param playerID The player whos score to save
	 * @param gameID The ID of the game which was played
	 * @param argumentCount The number of arguments in the va_list
	 * @param ... The score to save
	 */
	void SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, uint32_t argumentCount, ...);

	Leaderboard::Type GetLeaderboardType(const GameID gameID);
	LeaderboardCache leaderboardCache;
};

