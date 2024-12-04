#include "MySQLDatabase.h"

#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

std::optional<uint32_t> MySQLDatabase::GetDonationTotal(const uint32_t activityId) {
	auto donation_total = ExecuteSelect("SELECT SUM(primaryScore) as donation_total FROM leaderboard WHERE game_id = ?;", activityId);

	if (!donation_total->next()) {
		return std::nullopt;
	}

	return donation_total->getUInt("donation_total");
}

std::vector<ILeaderboard::Entry> ProcessQuery(UniqueResultSet& rows) {
	std::vector<ILeaderboard::Entry> entries;
	entries.reserve(rows->rowsCount());

	while (rows->next()) {
		auto& entry = entries.emplace_back();

		entry.charId = rows->getUInt("character_id");
		entry.lastPlayedTimestamp = rows->getUInt("lp_unix");
		entry.primaryScore = rows->getUInt("primaryScore");
		entry.secondaryScore = rows->getUInt("secondaryScore");
		entry.tertiaryScore = rows->getUInt("tertiaryScore");
		entry.numWins = rows->getUInt("numWins");
		entry.numTimesPlayed = rows->getUInt("timesPlayed");
		entry.name = rows->getString("char_name");
		// entry.ranking is never set because its calculated in leaderboard in code.
	}

	return entries;
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetDefaultLeaderboard(const uint32_t activityId) {
	auto leaderboard = ExecuteSelect("SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;", activityId);
	return ProcessQuery(leaderboard);
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetAgsLeaderboard(const uint32_t activityId) {
	auto query = Game::config->GetValue("classic_survival_scoring") == "1" ? 
	"SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;" : 
	"SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY secondaryscore DESC, primaryscore DESC, tertiaryScore DESC, last_played ASC;";
	auto leaderboard = ExecuteSelect(query, activityId);
	return ProcessQuery(leaderboard);
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetNsLeaderboard(const uint32_t activityId) {
	auto leaderboard = ExecuteSelect("SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore ASC, secondaryscore DESC, tertiaryScore ASC, last_played ASC;", activityId);
	return ProcessQuery(leaderboard);
}

void MySQLDatabase::SaveScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {

}

void MySQLDatabase::UpdateScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {

}

std::optional<ILeaderboard::Score> MySQLDatabase::GetPlayerScore(const uint32_t playerId, const uint32_t gameId) {
	return std::nullopt;
}

void MySQLDatabase::IncrementNumWins(const uint32_t playerId, const uint32_t gameId) {

}
