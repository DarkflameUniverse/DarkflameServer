#ifndef __LEADERBOARDMANAGER__H__
#define __LEADERBOARDMANAGER__H__

#include <map>
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

typedef uint32_t GameID;

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
	static const std::string_view GetColumns(Type leaderboardType);
	static const std::string_view GetInsertFormat(Type leaderboardType);
	static const std::string_view GetOrdering(Type leaderboardType);
private:
	inline void WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, LDFBaseData* data);

	// Returns true if the string needs formatting
	bool GetRankingQuery(std::string& lookupReturn) const;

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
	class Score {
	public:
		Score() {
			primaryScore = 0;
			secondaryScore = 0;
			tertiaryScore = 0;
		}
		Score(uint32_t primaryScore, uint32_t secondaryScore = 0, uint32_t tertiaryScore = 0) {
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
		void SetPrimaryScore(const uint32_t score) { primaryScore = score; }
		uint32_t GetPrimaryScore() const { return primaryScore; }
		
		void SetSecondaryScore(const uint32_t score) { secondaryScore = score; }
		uint32_t GetSecondaryScore() const { return secondaryScore; }

		void SetTertiaryScore(const uint32_t score) { tertiaryScore = score; }
		uint32_t GetTertiaryScore() const { return tertiaryScore; }
	private:
		uint32_t primaryScore;
		uint32_t secondaryScore;
		uint32_t tertiaryScore;
	};

	using LeaderboardCache = std::map<GameID, Leaderboard::Type>;
	void SendLeaderboard(GameID gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID, LWOOBJID targetID, uint32_t resultStart = 0, uint32_t resultEnd = 10);

	/**
	 * @brief Public facing Score saving method.  This method is simply a wrapper to ensure va_end is called properly.
	 *
	 * @param playerID The player whos score to save
	 * @param gameID The ID of the game which was played
	 * @param argumentCount The number of arguments in the va_list
	 * @param ... The score to save
	 */
	void SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, uint32_t primaryScore, uint32_t secondaryScore = 0, uint32_t tertiaryScore = 0);

	void GetLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID = LWOOBJID_EMPTY);
	std::string FormatInsert(const Leaderboard::Type& type, const Score& score, const bool useUpdate);

	Leaderboard::Type GetLeaderboardType(const GameID gameID);
	extern LeaderboardCache leaderboardCache;
};

#endif  //!__LEADERBOARDMANAGER__H__
