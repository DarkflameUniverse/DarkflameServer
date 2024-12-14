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
		entry.primaryScore = rows->getFloat("primaryScore");
		entry.secondaryScore = rows->getFloat("secondaryScore");
		entry.tertiaryScore = rows->getFloat("tertiaryScore");
		entry.numWins = rows->getUInt("numWins");
		entry.numTimesPlayed = rows->getUInt("timesPlayed");
		entry.name = rows->getString("char_name");
		// entry.ranking is never set because its calculated in leaderboard in code.
	}

	return entries;
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetDescendingLeaderboard(const uint32_t activityId) {
	auto leaderboard = ExecuteSelect("SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;", activityId);
	return ProcessQuery(leaderboard);
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetAscendingLeaderboard(const uint32_t activityId) {
	auto leaderboard = ExecuteSelect("SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore ASC, secondaryscore ASC, tertiaryScore ASC, last_played ASC;", activityId);
	return ProcessQuery(leaderboard);
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetAgsLeaderboard(const uint32_t activityId) {
	auto query = Game::config->GetValue("classic_survival_scoring") != "1" ? 
	"SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;" : 
	"SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY secondaryscore DESC, primaryscore DESC, tertiaryScore DESC, last_played ASC;";
	auto leaderboard = ExecuteSelect(query, activityId);
	return ProcessQuery(leaderboard);
}

std::vector<ILeaderboard::Entry> MySQLDatabase::GetNsLeaderboard(const uint32_t activityId) {
	auto leaderboard = ExecuteSelect("SELECT *, UNIX_TIMESTAMP(last_played) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore ASC, tertiaryScore DESC, last_played ASC;", activityId);
	return ProcessQuery(leaderboard);
}

void MySQLDatabase::SaveScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {
	ExecuteInsert("INSERT leaderboard SET primaryScore = ?, secondaryScore = ?, tertiaryScore = ?, character_id = ?, game_id = ?;",
		score.primaryScore, score.secondaryScore, score.tertiaryScore, playerId, gameId);
}

void MySQLDatabase::UpdateScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {
	ExecuteInsert("UPDATE leaderboard SET primaryScore = ?, secondaryScore = ?, tertiaryScore = ?, timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;",
		score.primaryScore, score.secondaryScore, score.tertiaryScore, playerId, gameId);
}

void MySQLDatabase::IncrementTimesPlayed(const uint32_t playerId, const uint32_t gameId) {
	ExecuteUpdate("UPDATE leaderboard SET timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;", playerId, gameId);
}

std::optional<ILeaderboard::Score> MySQLDatabase::GetPlayerScore(const uint32_t playerId, const uint32_t gameId) {
	std::optional<ILeaderboard::Score> toReturn = std::nullopt;
	auto res = ExecuteSelect("SELECT * FROM leaderboard WHERE character_id = ? AND game_id = ?;", playerId, gameId);
	if (res->next()) {
		toReturn = ILeaderboard::Score{
			.primaryScore = res->getFloat("primaryScore"),
			.secondaryScore = res->getFloat("secondaryScore"),
			.tertiaryScore = res->getFloat("tertiaryScore")
		};
	}

	return toReturn;
}

void MySQLDatabase::IncrementNumWins(const uint32_t playerId, const uint32_t gameId) {
	ExecuteUpdate("UPDATE leaderboard SET numWins = numWins + 1 WHERE character_id = ? AND game_id = ?;", playerId, gameId);
}
