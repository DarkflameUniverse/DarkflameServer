#include "SQLiteDatabase.h"

#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

std::optional<uint32_t> SQLiteDatabase::GetDonationTotal(const uint32_t activityId) {
	auto [_, donation_total] = ExecuteSelect("SELECT SUM(primaryScore) as donation_total FROM leaderboard WHERE game_id = ?;", activityId);

	if (donation_total.eof()) {
		return std::nullopt;
	}

	return donation_total.getIntField("donation_total");
}

std::vector<ILeaderboard::Entry> ProcessQuery(CppSQLite3Query& rows) {
	std::vector<ILeaderboard::Entry> entries;

	while (!rows.eof()) {
		auto& entry = entries.emplace_back();

		entry.charId = rows.getIntField("character_id");
		entry.lastPlayedTimestamp = rows.getIntField("lp_unix");
		entry.primaryScore = rows.getFloatField("primaryScore");
		entry.secondaryScore = rows.getFloatField("secondaryScore");
		entry.tertiaryScore = rows.getFloatField("tertiaryScore");
		entry.numWins = rows.getIntField("numWins");
		entry.numTimesPlayed = rows.getIntField("timesPlayed");
		entry.name = rows.getStringField("char_name");
		// entry.ranking is never set because its calculated in leaderboard in code.
		rows.nextRow();
	}

	return entries;
}

std::vector<ILeaderboard::Entry> SQLiteDatabase::GetDescendingLeaderboard(const uint32_t activityId) {
	auto [_, result] = ExecuteSelect("SELECT *, CAST(strftime('%s', last_played) as INT) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;", activityId);
	return ProcessQuery(result);
}

std::vector<ILeaderboard::Entry> SQLiteDatabase::GetAscendingLeaderboard(const uint32_t activityId) {
	auto [_, result] = ExecuteSelect("SELECT *, CAST(strftime('%s', last_played) as INT) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore ASC, secondaryscore ASC, tertiaryScore ASC, last_played ASC;", activityId);
	return ProcessQuery(result);
}

std::vector<ILeaderboard::Entry> SQLiteDatabase::GetAgsLeaderboard(const uint32_t activityId) {
	auto query = Game::config->GetValue("classic_survival_scoring") != "1" ? 
	"SELECT *, CAST(strftime('%s', last_played) as INT) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore DESC, tertiaryScore DESC, last_played ASC;" : 
	"SELECT *, CAST(strftime('%s', last_played) as INT) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY secondaryscore DESC, primaryscore DESC, tertiaryScore DESC, last_played ASC;";
	auto [_, result] = ExecuteSelect(query, activityId);
	return ProcessQuery(result);
}

std::vector<ILeaderboard::Entry> SQLiteDatabase::GetNsLeaderboard(const uint32_t activityId) {
	auto [_, result] = ExecuteSelect("SELECT *, CAST(strftime('%s', last_played) as INT) as lp_unix, ci.name as char_name FROM leaderboard lb JOIN charinfo ci on ci.id = lb.character_id where game_id = ? ORDER BY primaryscore DESC, secondaryscore ASC, tertiaryScore DESC, last_played ASC;", activityId);
	return ProcessQuery(result);
}

void SQLiteDatabase::SaveScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {
	ExecuteInsert("INSERT INTO leaderboard (primaryScore, secondaryScore, tertiaryScore, character_id, game_id, last_played) VALUES (?,?,?,?,?,CURRENT_TIMESTAMP) ;",
		score.primaryScore, score.secondaryScore, score.tertiaryScore, playerId, gameId);
}

void SQLiteDatabase::UpdateScore(const uint32_t playerId, const uint32_t gameId, const Score& score) {
	ExecuteInsert("UPDATE leaderboard SET primaryScore = ?, secondaryScore = ?, tertiaryScore = ?, timesPlayed = timesPlayed + 1, last_played = CURRENT_TIMESTAMP WHERE character_id = ? AND game_id = ?;",
		score.primaryScore, score.secondaryScore, score.tertiaryScore, playerId, gameId);
}

std::optional<ILeaderboard::Score> SQLiteDatabase::GetPlayerScore(const uint32_t playerId, const uint32_t gameId) {
	std::optional<ILeaderboard::Score> toReturn = std::nullopt;
	auto [_, res] = ExecuteSelect("SELECT * FROM leaderboard WHERE character_id = ? AND game_id = ?;", playerId, gameId);
	if (!res.eof()) {
		toReturn = ILeaderboard::Score{
			.primaryScore = static_cast<float>(res.getFloatField("primaryScore")),
			.secondaryScore = static_cast<float>(res.getFloatField("secondaryScore")),
			.tertiaryScore = static_cast<float>(res.getFloatField("tertiaryScore"))
		};
	}

	return toReturn;
}

void SQLiteDatabase::IncrementNumWins(const uint32_t playerId, const uint32_t gameId) {
	ExecuteUpdate("UPDATE leaderboard SET numWins = numWins + 1, last_played = CURRENT_TIMESTAMP WHERE character_id = ? AND game_id = ?;", playerId, gameId);
}

void SQLiteDatabase::IncrementTimesPlayed(const uint32_t playerId, const uint32_t gameId) {
	ExecuteUpdate("UPDATE leaderboard SET timesPlayed = timesPlayed + 1, last_played = CURRENT_TIMESTAMP WHERE character_id = ? AND game_id = ?;", playerId, gameId);
}
