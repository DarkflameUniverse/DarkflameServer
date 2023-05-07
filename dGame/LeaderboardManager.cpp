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
	std::ostringstream leaderboard;

	leaderboard << "ADO.Result=7:1\n"; // Unused in 1.10.64, but is in captures
	leaderboard << "Result.Count=1:1\n"; // number of results, always 1?
	leaderboard << "Result[0].Index=0:RowNumber\n"; // "Primary key"
	leaderboard << "Result[0].RowCount=1:" << entries.size() << '\n'; // number of rows

	int32_t index = 0;
	for (auto& entry : entries) {
		for (auto data : entry) {
			WriteLeaderboardRow(leaderboard, index, data);
		}
		index++;
	}

	// Serialize the thing to a BitStream
	bitStream->Write(leaderboard.str().c_str(), leaderboard.tellp());
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

void Leaderboard::SetupLeaderboard() {
	std::string queryBase =
	R"QUERY( 
	WITH leaderboardsRanked AS ( 
		SELECT leaderboard.*, charinfo.name, 
			RANK() OVER 
			( 
			ORDER BY %s 
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
	LEAST(GREATEST(CAST(myRank AS SIGNED) - 5, 1), lowestRanking.lowestRank - 10) 
	AND 
	LEAST(GREATEST(myRank + 5, 11), lowestRanking.lowestRank) 
	ORDER BY ranking ASC;)QUERY";
	// Setup query based on activity. 
	// Where clause will vary based on what query we are doing
	// Get base based on InfoType
	// Fill in base with arguments based on leaderboard type
	// If this is a friends query we need to join another table and add even more to the where clause.

	const char* friendsQuery =
		" AND (character_id IN (SELECT fr.requested_player FROM (SELECT CASE "
		"WHEN player_id = ? THEN friend_id "
		"WHEN friend_id = ? THEN player_id "
		"END AS requested_player FROM friends) AS fr "
		"JOIN charinfo AS ci ON ci.id = fr.requested_player "
		"WHERE fr.requested_player IS NOT NULL) OR character_id = ?) ";

	std::string orderBase;
	std::string selectBase;
	switch (leaderboardType) {
	case Type::ShootingGallery: {
		orderBase = "score DESC, streak DESC, hitPercentage DESC";
		selectBase = "hitPercentage, score, streak";
		break;
	}
	case Type::Racing:
		orderBase = "bestTime ASC, bestLapTime ASC, numWins DESC";
		selectBase = "bestLapTime, bestTime, numWins";
		break;
	case Type::UnusedLeaderboard4:
		orderBase = "score DESC";
		selectBase = "score";
		break;
	case Type::MonumentRace:
		orderBase = "bestTime ASC";
		selectBase = "bestTime";
		break;
	case Type::FootRace:
		orderBase = "bestTime DESC";
		selectBase = "bestTime";
		break;
	case Type::Survival:
		orderBase = "score DESC, bestTime DESC";
		selectBase = "score, bestTime";
		// If the config option default_survival_scoring is 1, reverse the order of the points and time columns
		break;
	case Type::SurvivalNS:
		orderBase = "bestTime DESC, score DESC";
		selectBase = "bestTime, score";
		break;
	case Type::Donations:
		orderBase = "score DESC";
		selectBase = "score";
		break;
	case Type::None:
		Game::logger->Log("LeaderboardManager", "Attempting to get leaderboard for type none.	Is this intended?");
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		break;
	}

	constexpr uint16_t STRING_LENGTH = 1526;
	char lookupBuffer[STRING_LENGTH];
	if (this->infoType == InfoType::Friends) snprintf(lookupBuffer, STRING_LENGTH, queryBase.c_str(), orderBase.c_str(), friendsQuery, selectBase.c_str());
	else snprintf(lookupBuffer, STRING_LENGTH, queryBase.c_str(), orderBase.c_str(), "", selectBase.c_str());

	std::string baseLookupStr;
	char baseRankingBuffer[STRING_LENGTH];
	bool neededFormatting = GetRankingQuery(baseLookupStr);
	if (neededFormatting) snprintf(baseRankingBuffer, STRING_LENGTH, baseLookupStr.c_str(), orderBase.c_str());
	else std::copy(baseLookupStr.begin(), baseLookupStr.end() + 1, baseRankingBuffer);

	Game::logger->Log("LeaderboardManager", "lookup query is %s", baseRankingBuffer);
	std::unique_ptr<sql::PreparedStatement> baseQuery(Database::CreatePreppedStmt(baseRankingBuffer));
	baseQuery->setInt(1, this->gameID);
	if (!neededFormatting) {
		baseQuery->setInt(2, this->relatedPlayer);
	}

	std::unique_ptr<sql::ResultSet> baseResult(baseQuery->executeQuery());
	if (!baseResult->next()) return;
	// Get the ID of the row fetched.
	uint32_t relatedPlayerLeaderboardId = baseResult->getInt("id");

	// create and execute query here
	Game::logger->Log("LeaderboardManager", "filled in query is %s %i %i %i", lookupBuffer, this->gameID, this->relatedPlayer, relatedPlayerLeaderboardId);
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

	if (result->rowsCount() == 0) return;

	this->entries.reserve(result->rowsCount());
	while (result->next()) {
		constexpr int32_t MAX_NUM_DATA_PER_ROW = 9;
		this->entries.push_back(std::vector<LDFBaseData*>());
		auto& entry = this->entries.back();
		entry.reserve(MAX_NUM_DATA_PER_ROW);
		entry.push_back(new LDFData<uint64_t>(u"CharacterID", result->getInt("character_id")));
		entry.push_back(new LDFData<uint64_t>(u"LastPlayed", result->getUInt64("lastPlayed")));
		entry.push_back(new LDFData<int32_t>(u"NumPlayed", 1));
		entry.push_back(new LDFData<std::u16string>(u"name", GeneralUtils::ASCIIToUTF16(result->getString("name").c_str())));
		entry.push_back(new LDFData<int32_t>(u"RowNumber", result->getInt("ranking")));
		switch (leaderboardType) {
		case Type::ShootingGallery:
			entry.push_back(new LDFData<float>(u"HitPercentage", result->getDouble("hitPercentage")));
			// HitPercentage:3 between 0 and 1
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Score:1
			entry.push_back(new LDFData<int32_t>(u"Streak", result->getInt("streak")));
			// Streak:1
			break;
		case Type::Racing:
			entry.push_back(new LDFData<float>(u"BestLapTime", result->getDouble("bestLapTime")));
			// BestLapTime:3
			entry.push_back(new LDFData<float>(u"BestTime", result->getDouble("bestTime")));
			// BestTime:3
			entry.push_back(new LDFData<int32_t>(u"License", 1));
			// License:1 - 1 if player has completed mission 637 and 0 otherwise
			entry.push_back(new LDFData<int32_t>(u"NumWins", result->getInt("numWins")));
			// NumWins:1
			break;
		case Type::UnusedLeaderboard4:
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Points:1
			break;
		case Type::MonumentRace:
			entry.push_back(new LDFData<int32_t>(u"Time", result->getInt("bestTime")));
			// Time:1(?)
			break;
		case Type::FootRace:
			entry.push_back(new LDFData<int32_t>(u"Time", result->getInt("bestTime")));
			// Time:1
			break;
		case Type::Survival:
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Points:1
			entry.push_back(new LDFData<int32_t>(u"Time", result->getInt("bestTime")));
			// Time:1
			break;
		case Type::SurvivalNS:
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Wave:1
			entry.push_back(new LDFData<int32_t>(u"Time", result->getInt("bestTime")));
			// Time:1
			break;
		case Type::Donations:
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Score:1				
			break;
		case Type::None:
			// This type is included here simply to resolve a compiler warning on mac about unused enum types
			break;
		default:
			break;
		}
	}
	for (auto& entry : entries) {
		for (auto data : entry) {
			Game::logger->Log("LeaderboardManager", "entry is %s", data->GetString().c_str());
		}
	}
}

void Leaderboard::Send(LWOOBJID targetID) const {
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

std::string FormatInsert(const std::string& columns, const std::string& format, va_list args, bool update) {
	const char* insert = "INSERT";
	const char* update = "UPDATE";
	auto queryBase = "%s leaderboard SET %s WHERE id = ?;";
	constexpr uint16_t STRING_LENGTH = 400;
	char formattedInsert[STRING_LENGTH];
	char finishedQuery[STRING_LENGTH];
	snprintf(formattedInsert, STRING_LENGTH, queryBase, columns.c_str(), format.c_str());
	vsnprintf(finishedQuery, STRING_LENGTH, formattedInsert, args);
	return finishedQuery;
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, va_list args) {
	std::string insertStatement;
	std::string selectedColumns;
	std::string insertFormat;
	// If ResultSet is empty, just insert our score.
	std::va_list argsCopy;
	va_copy(argsCopy, args);

	switch (leaderboardType) {
	case Leaderboard::Type::ShootingGallery: {
		selectedColumns = "score, hitPercentage, streak";
		insertFormat = "score=%i, hitPercentage=%f, streak=%i";
		break;
	}
	case Leaderboard::Type::Racing: {
		selectedColumns = "bestLapTime, bestTime";
		insertFormat = "bestLapTime=%f, bestTime=%f";
		break;
	}
	case Leaderboard::Type::UnusedLeaderboard4: {
		selectedColumns = "score";
		insertFormat = "score=%i";
		break;
	}
	case Leaderboard::Type::MonumentRace:
	case Leaderboard::Type::FootRace: {
		selectedColumns = "time";
		insertFormat = "time=%i";
		break;
	}
	case Leaderboard::Type::Survival: {
		selectedColumns = "points, time";
		insertFormat = "points=%i, time=%i";
		break;
	}
	case Leaderboard::Type::SurvivalNS: {
		selectedColumns = "time, wave";
		insertFormat = "time=%i, wave=%i";
		break;
	}
	case Leaderboard::Type::Donations: {
		selectedColumns = "score";
		insertFormat = "score=%i";
		break;
	}
	case Leaderboard::Type::None: {
		Game::logger->Log("LeaderboardManager", "Warning: Saving leaderboard of type None. Are you sure this is intended?");
		break;
	}
	default: {
		Game::logger->Log("LeaderboardManager", "Unknown leaderboard type %i.	Cannot save score!", leaderboardType);
		return;
	}
	}
	const char* lookup = "SELECT %s FROM leaderboard WHERE character_id = ? AND game_id = ?;";
	constexpr uint16_t STRING_LENGTH = 400;
	char lookupBuffer[STRING_LENGTH];
	snprintf(lookupBuffer, STRING_LENGTH, lookup, selectedColumns.c_str());
	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt(lookupBuffer));
	query->setInt(1, playerID);
	query->setInt(2, gameID);
	std::unique_ptr<sql::ResultSet> myScoreResult(query->executeQuery());

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

		if (
			score > oldScore || // If score is better
			(score == oldScore && hitPercentage > oldHitPercentage) || // or if the score is tied and the hitPercentage is better
			(score == oldScore && hitPercentage == oldHitPercentage && streak > oldStreak)) { // or if the score and hitPercentage are tied and the streak is better
			// Save
		}
		break;
	}
	case Leaderboard::Type::Racing: {
		float oldLapTime = myScoreResult->getFloat("bestLapTime");
		float lapTime;
		lapTime = va_arg(argsCopy, double);

		float oldTime = myScoreResult->getFloat("bestTime");
		float oldTime;
		oldTime = va_arg(argsCopy, double);

		int32_t oldNumWins = myScoreResult->getInt("numWins");
		bool won;
		won = va_arg(argsCopy, int32_t);
		// Compare bestTime, if LOWER save
		// Compare bestLapTime, if LOWER save
		// Increment numWins if player won
		break;
	}
	case Leaderboard::Type::UnusedLeaderboard4: {
		int32_t oldScore = myScoreResult->getInt("score");
		int32_t points;
		points = va_arg(argsCopy, int32_t);
		// Compare score, if HIGHER save
		break;
	}
	case Leaderboard::Type::MonumentRace: {
		int32_t oldTime = myScoreResult->getInt("time");
		int32_t time;
		time = va_arg(argsCopy, int32_t);
		// Compare time, if LOWER save
		break;
	}
	case Leaderboard::Type::FootRace: {
		int32_t oldTime = myScoreResult->getInt("time");
		int32_t time;
		time = va_arg(argsCopy, int32_t);
		// Compare time, if HIGHER save
		break;
	}
	case Leaderboard::Type::Survival: {
		int32_t oldPoints = myScoreResult->getInt("points");
		int32_t points;
		points = va_arg(argsCopy, int32_t);

		int32_t oldTime = myScoreResult->getInt("time");
		int32_t time;
		time = va_arg(argsCopy, int32_t);
		// Compare points, if HIGHER save, if TIED compare time, if LOWER save
		// If classic_survival_scoring is 1, reverse the order of the points and time columns
		break;
	}
	case Leaderboard::Type::SurvivalNS: {
		int32_t oldTime = myScoreResult->getInt("time");
		int32_t time;
		time = va_arg(argsCopy, int32_t);

		int32_t oldWave = myScoreResult->getInt("wave");
		int32_t wave;
		wave = va_arg(argsCopy, int32_t);
		// Compare wave, if HIGHER save, if TIED compare time, if LOWER save
		break;
	}
	case Leaderboard::Type::Donations: {
		int32_t oldScore = myScoreResult->getInt("score");
		int32_t score;
		score = va_arg(argsCopy, int32_t);
		// Compare score, if HIGHER save
		break;
	}
	case Leaderboard::Type::None: {
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		Game::logger->Log("LeaderboardManager", "Warning: Saving score for leaderboard of type None. Are you sure this is intended?");
		break;
	}
	default:
		Game::logger->Log("LeaderboardManager", "Unknown leaderboard type %i.	Cannot save score!", leaderboardType);
		break;
	}
	va_end(argsCopy);
}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID targetID, LWOOBJID playerID) {
	Leaderboard leaderboard(gameID, infoType, weekly, playerID, GetLeaderboardType(gameID));
	leaderboard.SetupLeaderboard();
	leaderboard.Send(targetID);
}

// Done
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
