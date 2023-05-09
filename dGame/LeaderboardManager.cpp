#include "LeaderboardManager.h"
#include <utility>
#include "Database.h"
#include "EntityManager.h"
#include "Character.h"
#include "Game.h"
#include "GameMessages.h"
#include "dLogger.h"
#include "dConfig.h"
#include "CDClientManager.h"
#include "GeneralUtils.h"
#include "Entity.h"
#include "LDFFormat.h"
#include <sstream>

#include "CDActivitiesTable.h"
#include "Metrics.hpp"

Leaderboard::Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const Leaderboard::Type leaderboardType) {
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->leaderboardType = leaderboardType;
	this->relatedPlayer = relatedPlayer;
}

Leaderboard::~Leaderboard() {
	for (auto& entry : entries) for (auto data : entry) delete data;
}

void Leaderboard::WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, LDFBaseData* data) {
	leaderboard << "Result[0].Row[" << index << "]." << data->GetString() << '\n';
}

void Leaderboard::Serialize(RakNet::BitStream* bitStream) {
	bitStream->Write(gameID);
	bitStream->Write(leaderboardType);

	std::ostringstream leaderboard;

	leaderboard << "ADO.Result=7:1\n"; // Unused in 1.10.64, but is in captures
	leaderboard << "Result.Count=1:1\n"; // number of results, always 1?
	leaderboard << "Result[0].Index=0:RowNumber\n"; // "Primary key"
	leaderboard << "Result[0].RowCount=1:" << entries.size() << '\n'; // number of rows

	int32_t rowNumber = 0;
	for (auto& entry : entries) {
		for (auto* data : entry) {
			WriteLeaderboardRow(leaderboard, rowNumber, data);
		}
		rowNumber++;
	}

	// Serialize the thing to a BitStream
	bitStream->WriteAlignedBytes((const unsigned char*)leaderboard.str().c_str(), leaderboard.tellp());
	bitStream->Write0();
	bitStream->Write0();
}

bool Leaderboard::GetRankingQuery(std::string& lookupReturn) const {
	if (this->infoType == InfoType::Top) {
		lookupReturn = "SELECT id FROM leaderboard WHERE game_id = ? ORDER BY %s LIMIT 1";
		return true;
	} else {
		lookupReturn = "SELECT id FROM leaderboard WHERE game_id = ? AND character_id = ? LIMIT 1";
		return false;
	}
}

void Leaderboard::QueryToLdf(std::unique_ptr<sql::ResultSet>& rows) {
	if (rows->rowsCount() == 0) return;

	this->entries.reserve(rows->rowsCount());
	while (rows->next()) {
		constexpr int32_t MAX_NUM_DATA_PER_ROW = 9;
		this->entries.push_back(std::vector<LDFBaseData*>());
		auto& entry = this->entries.back();
		entry.reserve(MAX_NUM_DATA_PER_ROW);
		entry.push_back(new LDFData<uint64_t>(u"CharacterID", rows->getInt("character_id")));
		entry.push_back(new LDFData<uint64_t>(u"LastPlayed", rows->getUInt64("lastPlayed")));
		entry.push_back(new LDFData<int32_t>(u"NumPlayed", 1));
		entry.push_back(new LDFData<std::u16string>(u"name", GeneralUtils::ASCIIToUTF16(rows->getString("name").c_str())));
		entry.push_back(new LDFData<int32_t>(u"RowNumber", rows->getInt("ranking")));
		switch (leaderboardType) {
		case Type::ShootingGallery:
			entry.push_back(new LDFData<float>(u"HitPercentage", rows->getDouble("hitPercentage")));
			// HitPercentage:3 between 0 and 1
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("score")));
			// Score:1
			entry.push_back(new LDFData<int32_t>(u"Streak", rows->getInt("streak")));
			// Streak:1
			break;
		case Type::Racing:
			entry.push_back(new LDFData<float>(u"BestLapTime", rows->getDouble("bestLapTime")));
			// BestLapTime:3
			entry.push_back(new LDFData<float>(u"BestTime", rows->getDouble("bestTime")));
			// BestTime:3
			entry.push_back(new LDFData<int32_t>(u"License", 1));
			// License:1 - 1 if player has completed mission 637 and 0 otherwise
			entry.push_back(new LDFData<int32_t>(u"NumWins", rows->getInt("numWins")));
			// NumWins:1
			break;
		case Type::UnusedLeaderboard4:
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("score")));
			// Points:1
			break;
		case Type::MonumentRace:
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("bestTime")));
			// Time:1(?)
			break;
		case Type::FootRace:
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("bestTime")));
			// Time:1
			break;
		case Type::Survival:
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("score")));
			// Points:1
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("bestTime")));
			// Time:1
			break;
		case Type::SurvivalNS:
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("score")));
			// Wave:1
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("bestTime")));
			// Time:1
			break;
		case Type::Donations:
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("score")));
			// Score:1				
			break;
		case Type::None:
			// This type is included here simply to resolve a compiler warning on mac about unused enum types
			break;
		default:
			break;
		}
	}
}

std::string Leaderboard::GetColumns(Leaderboard::Type leaderboardType) {
	std::string columns;
	switch (leaderboardType) {
	case Type::ShootingGallery:
		columns = "hitPercentage, score, streak";
		break;
	case Type::Racing:
		columns = "bestLapTime, bestTime, numWins";
		break;
	case Type::UnusedLeaderboard4:
		columns = "score";
		break;
	case Type::MonumentRace:
		columns = "bestTime";
		break;
	case Type::FootRace:
		columns = "bestTime";
		break;
	case Type::Survival:
		columns = "bestTime, score";
		break;
	case Type::SurvivalNS:
		columns = "bestTime, score";
		break;
	case Type::Donations:
		columns = "score";
		break;
	case Type::None:
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		break;
	}
	return columns;
}

std::string Leaderboard::GetInsertFormat(Leaderboard::Type leaderboardType) {
	std::string columns;
	switch (leaderboardType) {
	case Type::ShootingGallery:
		columns = "hitPercentage=%f, score=%i, streak=%i";
		break;
	case Type::Racing:
		columns = "bestLapTime=%i, bestTime=%i, numWins=numWins + %i";
		break;
	case Type::UnusedLeaderboard4:
		columns = "score=%i";
		break;
	case Type::MonumentRace:
		columns = "bestTime=%i";
		break;
	case Type::FootRace:
		columns = "bestTime=%i";
		break;
	case Type::Survival:
		columns = "bestTime=%i, score=%i";
		break;
	case Type::SurvivalNS:
		columns = "bestTime=%i, score=%i";
		break;
	case Type::Donations:
		columns = "score=%i";
		break;
	case Type::None:
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		break;
	}
	return columns;
}

std::string Leaderboard::GetOrdering(Leaderboard::Type leaderboardType) {
	std::string orderBase;
	switch (leaderboardType) {
	case Type::ShootingGallery:
		orderBase = "score DESC, streak DESC, hitPercentage DESC";
		break;
	case Type::Racing:
		orderBase = "bestTime ASC, bestLapTime ASC, numWins DESC";
		break;
	case Type::UnusedLeaderboard4:
		orderBase = "score DESC";
		break;
	case Type::MonumentRace:
		orderBase = "bestTime ASC";
		break;
	case Type::FootRace:
		orderBase = "bestTime DESC";
		break;
	case Type::Survival:
		orderBase = "score DESC, bestTime DESC";
		break;
	case Type::SurvivalNS:
		orderBase = "bestTime DESC, score DESC";
		break;
	case Type::Donations:
		orderBase = "score DESC";
		break;
	case Type::None:
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		break;
	}
	return orderBase;
}

void Leaderboard::SetupLeaderboard(uint32_t resultStart, uint32_t resultEnd) {
	resultStart++;
	resultEnd++;
	std::string queryBase =
		R"QUERY( 
		WITH leaderboardsRanked AS ( 
			SELECT leaderboard.*, charinfo.name, 
				RANK() OVER 
				( 
				ORDER BY %s, UNIX_TIMESTAMP(last_played) ASC, id DESC
			) AS ranking 
				FROM leaderboard JOIN charinfo on charinfo.id = leaderboard.character_id 
				WHERE game_id = ? %s 
		), 
		myStanding AS ( 
			SELECT 
				ranking as myRank 
			FROM leaderboardsRanked 
			WHERE id = ? 
		), 
		lowestRanking AS ( 
			SELECT MAX(ranking) AS lowestRank 
				FROM leaderboardsRanked 
		) 
		SELECT %s, character_id, UNIX_TIMESTAMP(last_played) as lastPlayed, leaderboardsRanked.name, leaderboardsRanked.ranking FROM leaderboardsRanked, myStanding, lowestRanking 
		WHERE leaderboardsRanked.ranking 
		BETWEEN 
		LEAST(GREATEST(CAST(myRank AS SIGNED) - 5, %i), lowestRanking.lowestRank - 10) 
		AND 
		LEAST(GREATEST(myRank + 5, %i), lowestRanking.lowestRank) 
		ORDER BY ranking ASC;
	)QUERY";

	const char* friendsQuery =
		R"QUERY( AND (
		character_id IN (
			SELECT fr.requested_player FROM (
				SELECT CASE
				WHEN player_id = ? THEN friend_id
				WHEN friend_id = ? THEN player_id
				END AS requested_player
				FROM friends
			) AS fr
			JOIN charinfo AS ci
			ON ci.id = fr.requested_player
			WHERE fr.requested_player IS NOT NULL
		)
		OR character_id = ?
		)
	)QUERY";

	std::string orderBase = GetOrdering(this->leaderboardType);
	std::string selectBase = GetColumns(this->leaderboardType);

	constexpr uint16_t STRING_LENGTH = 1526;
	char lookupBuffer[STRING_LENGTH];
	// If we are getting the friends leaderboard, add the friends query, otherwise fill it in with nothing.
	if (this->infoType == InfoType::Friends) {
		snprintf(lookupBuffer, STRING_LENGTH, queryBase.c_str(),
		orderBase.c_str(), friendsQuery, selectBase.c_str(), resultStart, resultEnd);
	}
	else {
		snprintf(lookupBuffer, STRING_LENGTH, queryBase.c_str(),
		orderBase.c_str(), "", selectBase.c_str(), resultStart, resultEnd);
	}

	std::string baseLookupStr;
	char baseRankingBuffer[STRING_LENGTH];
	bool neededFormatting = GetRankingQuery(baseLookupStr);

	// If we need to format the base ranking query, do so, otherwise just copy the query since it's already formatted.
	if (neededFormatting) snprintf(baseRankingBuffer, STRING_LENGTH, baseLookupStr.c_str(), orderBase.c_str());
	else std::copy(baseLookupStr.begin(), baseLookupStr.end() + 1, baseRankingBuffer);

	std::unique_ptr<sql::PreparedStatement> baseQuery(Database::CreatePreppedStmt(baseRankingBuffer));
	baseQuery->setInt(1, this->gameID);
	if (!neededFormatting) {
		baseQuery->setInt(2, this->relatedPlayer);
	}

	std::unique_ptr<sql::ResultSet> baseResult(baseQuery->executeQuery());
	if (!baseResult->next()) return; // In this case, there are no entries in the leaderboard for this game.

	uint32_t relatedPlayerLeaderboardId = baseResult->getInt("id");

	// Create and execute the actual save here
	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt(lookupBuffer));

	query->setInt(1, this->gameID);
	if (this->infoType == InfoType::Friends) {
		query->setInt(2, this->relatedPlayer);
		query->setInt(3, this->relatedPlayer);
		query->setInt(4, this->relatedPlayer);
		query->setInt(5, relatedPlayerLeaderboardId);
	} else {
		query->setInt(2, relatedPlayerLeaderboardId);
	}

	std::unique_ptr<sql::ResultSet> result(query->executeQuery());
	QueryToLdf(result);
}

void Leaderboard::Send(LWOOBJID targetID) {
	auto* player = EntityManager::Instance()->GetEntity(relatedPlayer);
	if (player != nullptr) {
		GameMessages::SendActivitySummaryLeaderboardData(targetID, this, player->GetSystemAddress());
	}
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, uint32_t argumentCount, ...) {
	va_list args;
	va_start(args, argumentCount);
	SaveScore(playerID, gameID, leaderboardType, args);
	va_end(args);
}

std::string FormatInsert(const std::string& columns, const std::string& format, va_list args, bool useUpdate) {
	const char* insertClause = "INSERT";
	const char* updateClause = "UPDATE";
	const char* queryType = useUpdate ? updateClause : insertClause;

	const char* insertFilter = ", character_id = ?, game_id = ?, timesPlayed = 1";
	const char* updateFilter = ", timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?";
	const char* usedFilter = useUpdate ? updateFilter : insertFilter;

	constexpr uint16_t STRING_LENGTH = 400;
	char formattedInsert[STRING_LENGTH];
	auto queryBase = "%s leaderboard SET %s %s;";
	snprintf(formattedInsert, STRING_LENGTH, queryBase, queryType, format.c_str(), usedFilter);

	char finishedQuery[STRING_LENGTH];
	vsnprintf(finishedQuery, STRING_LENGTH, formattedInsert, args);
	return finishedQuery;
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, va_list activityScore) {
	std::string selectedColumns = Leaderboard::GetColumns(leaderboardType);
	std::string insertFormat = Leaderboard::GetInsertFormat(leaderboardType);
	const char* lookup = "SELECT %s FROM leaderboard WHERE character_id = ? AND game_id = ?;";

	constexpr uint16_t STRING_LENGTH = 400;
	char lookupBuffer[STRING_LENGTH];
	snprintf(lookupBuffer, STRING_LENGTH, lookup, selectedColumns.c_str());

	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt(lookupBuffer));
	query->setInt(1, playerID);
	query->setInt(2, gameID);
	std::unique_ptr<sql::ResultSet> myScoreResult(query->executeQuery());

	std::va_list argsCopy;
	va_copy(argsCopy, activityScore);
	std::string saveQuery;
	if (myScoreResult->next()) {
		switch (leaderboardType) {
		case Leaderboard::Type::ShootingGallery: {
			int32_t oldScore = myScoreResult->getInt("score");
			int32_t score;
			score = va_arg(argsCopy, int32_t);

			float oldHitPercentage = myScoreResult->getFloat("hitPercentage");
			float hitPercentage;
			hitPercentage = va_arg(argsCopy, double);

			int32_t oldStreak = myScoreResult->getInt("streak");
			int32_t streak;
			streak = va_arg(argsCopy, int32_t);

			if (score > oldScore || // If score is better
				(score == oldScore && hitPercentage > oldHitPercentage) || // or if the score is tied and the hitPercentage is better
				(score == oldScore && hitPercentage == oldHitPercentage && streak > oldStreak)) { // or if the score and hitPercentage are tied and the streak is better
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::Racing: {
			uint32_t oldLapTime = myScoreResult->getFloat("bestLapTime");
			uint32_t lapTime;
			lapTime = va_arg(argsCopy, uint32_t);

			uint32_t oldTime = myScoreResult->getFloat("bestTime");
			uint32_t newTime;
			newTime = va_arg(argsCopy, uint32_t);

			bool won;
			won = va_arg(argsCopy, int32_t);

			if (newTime < oldTime ||
				(newTime == oldTime && lapTime < oldLapTime)) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			} else if (won) {
				std::unique_ptr<sql::PreparedStatement> incrementStatement(Database::CreatePreppedStmt("UPDATE leaderboard SET numWins = numWins + 1 WHERE character_id = ? AND game_id = ?;"));
				incrementStatement->setInt(1, playerID);
				incrementStatement->setInt(2, gameID);
				incrementStatement->executeUpdate();
			}
			break;
		}
		case Leaderboard::Type::UnusedLeaderboard4: {
			int32_t oldScore = myScoreResult->getInt("score");
			int32_t points;
			points = va_arg(argsCopy, int32_t);

			if (points > oldScore) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::MonumentRace: {
			int32_t oldTime = myScoreResult->getInt("bestTime");
			int32_t time;
			time = va_arg(argsCopy, int32_t);

			if (time < oldTime) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::FootRace: {
			int32_t oldTime = myScoreResult->getInt("bestTime");
			int32_t time;
			time = va_arg(argsCopy, int32_t);

			if (time < oldTime) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::Survival: {
			int32_t oldTime = myScoreResult->getInt("bestTime");
			int32_t time;
			time = va_arg(argsCopy, int32_t);

			int32_t oldPoints = myScoreResult->getInt("score");
			int32_t points;
			points = va_arg(argsCopy, int32_t);

			if (points > oldPoints || (points == oldPoints && time < oldTime)) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::SurvivalNS: {
			int32_t oldTime = myScoreResult->getInt("bestTime");
			int32_t time;
			time = va_arg(argsCopy, int32_t);

			int32_t oldWave = myScoreResult->getInt("score");
			int32_t wave;
			wave = va_arg(argsCopy, int32_t);

			if (time < oldTime || (time == oldTime && wave > oldWave)) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::Donations: {
			int32_t oldScore = myScoreResult->getInt("score");
			int32_t score;
			score = va_arg(argsCopy, int32_t);

			if (score > oldScore) {
				saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, true);
			}
			break;
		}
		case Leaderboard::Type::None:
		default:
			Game::logger->Log("LeaderboardManager", "Unknown leaderboard type %i.	Cannot save score!", leaderboardType);
			break;
		}
	} else {
		saveQuery = FormatInsert(selectedColumns, insertFormat, activityScore, false);
	}
	std::unique_ptr<sql::PreparedStatement> saveStatement;
	if (!saveQuery.empty()) {
		Game::logger->Log("LeaderboardManager", "Executing update with query %s", saveQuery.c_str());
		std::unique_ptr<sql::PreparedStatement> updateStatement(Database::CreatePreppedStmt(saveQuery));
		saveStatement = std::move(updateStatement);
	} else {
		Game::logger->Log("LeaderboardManager", "No new score to save, incrementing numTimesPlayed");
		// Increment the numTimes this player has played this game.
		std::unique_ptr<sql::PreparedStatement> updateStatement(Database::CreatePreppedStmt("UPDATE leaderboard SET timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;"));
		saveStatement = std::move(updateStatement);
	}
	saveStatement->setInt(1, playerID);
	saveStatement->setInt(2, gameID);
	saveStatement->execute();
	va_end(argsCopy);
}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID playerID, uint32_t resultStart, uint32_t resultEnd) {
	Leaderboard leaderboard(gameID, infoType, weekly, playerID, GetLeaderboardType(gameID));
	leaderboard.SetupLeaderboard(resultStart, resultEnd);
	leaderboard.Send(playerID);
}

Leaderboard::Type LeaderboardManager::GetLeaderboardType(const GameID gameID) {
	auto lookup = leaderboardCache.find(gameID);
	if (lookup != leaderboardCache.end()) return lookup->second;

	auto* activitiesTable = CDClientManager::Instance().GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([=](const CDActivities& entry) {
		return (entry.ActivityID == gameID);
		});
	auto type = activities.empty() ? static_cast<Leaderboard::Type>(activities.at(0).leaderboardType) : Leaderboard::Type::None;
	leaderboardCache.insert_or_assign(gameID, type);
	return type;
}
