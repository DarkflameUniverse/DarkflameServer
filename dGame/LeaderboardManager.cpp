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

#define MAX_QUERY_LENGTH 1526

void Leaderboard::SetupLeaderboard() {
	bool isTopQuery = this->infoType == InfoType::Top;
	bool isMyStandingQuery = this->infoType == InfoType::MyStanding;
	bool isFriendsQuery = this->infoType == InfoType::Friends;
	std::string baseLookupStr;

	if (!isTopQuery) {
		baseLookupStr = "SELECT id FROM leaderboard WHERE game_id = ? AND character_id = ? LIMIT 1";
	} else {
		baseLookupStr = "SELECT id FROM leaderboard WHERE game_id = ? ORDER BY %s LIMIT 1";
	}

	std::string queryBase =
		" \
	WITH leaderboardsRanked AS ( \
		SELECT leaderboard.*, charinfo.name, \
			RANK() OVER \
			( \
			ORDER BY %s \
		) AS ranking \
			FROM leaderboard JOIN charinfo on charinfo.id = leaderboard.character_id \
			WHERE game_id = ? %s \
	), \
	myStanding AS ( \
		SELECT \
			ranking as myRank \
		FROM leaderboardsRanked \
		WHERE id = ? \
	), \
	lowestRanking AS ( \
		SELECT MAX(ranking) AS lowestRank \
			FROM leaderboardsRanked \
	) \
	SELECT %s, character_id, UNIX_TIMESTAMP(last_played) as lastPlayed, leaderboardsRanked.name FROM leaderboardsRanked, myStanding, lowestRanking \
	WHERE leaderboardsRanked.ranking \
	BETWEEN \
	LEAST(GREATEST(myRank - 5, 1), lowestRanking.lowestRank - 10) \
	AND \
	LEAST(GREATEST(myRank + 5, 11), lowestRanking.lowestRank) \
	ORDER BY ranking ASC;";
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

	char baseStandingBuffer[1024];
	char lookupBuffer[MAX_QUERY_LENGTH];

	switch (leaderboardType) {
	case Type::ShootingGallery: {
		const char* orderBase = "score DESC, streak DESC, hitPercentage DESC";
		const char* selectBase = "hitPercentage, score, streak";
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), orderBase, selectBase);
		if (isFriendsQuery) snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), orderBase, friendsQuery, selectBase);
		else snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), orderBase, "", selectBase);
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), orderBase);
		break;
	}
	case Type::Racing:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "bestTime ASC, bestLapTime ASC, numWins DESC", "bestLapTime, bestTime, numWins");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "bestTime ASC, bestLapTime ASC, numWins DESC");
		break;
	case Type::UnusedLeaderboard4:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "score DESC", "score");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "score DESC");
		break;
	case Type::MonumentRace:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "bestTime ASC", "bestTime");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "bestTime ASC");
		break;
	case Type::FootRace:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "bestTime DESC", "bestTime");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "bestTime DESC");
		break;
	case Type::Survival:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "score DESC, bestTime DESC", "score, bestTime");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "score DESC, bestTime DESC");
		// If the config option default_survival_scoring is 1, reverse the order of the points and time columns
		break;
	case Type::SurvivalNS:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "bestTime DESC, score DESC", "bestTime, score");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "bestTime DESC, score DESC");
		break;
	case Type::Donations:
		snprintf(lookupBuffer, MAX_QUERY_LENGTH, queryBase.c_str(), "score DESC", "score");
		if (isTopQuery) snprintf(baseStandingBuffer, 1024, baseLookupStr.c_str(), "score DESC");
		break;
	case Type::None:
		Game::logger->Log("LeaderboardManager", "Attempting to get leaderboard for type none.	Is this intended?");
		// This type is included here simply to resolve a compiler warning on mac about unused enum types
		break;
	}
	Game::logger->Log("LeaderboardManager", "lookup query is %s", (!isTopQuery) ? baseLookupStr.c_str() : baseStandingBuffer);
	std::unique_ptr<sql::PreparedStatement> baseQuery(Database::CreatePreppedStmt((!isTopQuery) ? baseLookupStr : baseStandingBuffer));
	baseQuery->setInt(1, this->gameID);
	if (!isTopQuery) {
		baseQuery->setInt(2, this->relatedPlayer);
	}

	std::unique_ptr<sql::ResultSet> baseResult(baseQuery->executeQuery());
	if (baseResult->rowsCount() == 0) return;
	baseResult->next();
	// Get the ID of the row fetched.
	uint32_t relatedPlayerLeaderboardId = baseResult->getInt("id");

	// create and execute query here
	Game::logger->Log("LeaderboardManager", "filled in query is %s %i %i %i", lookupBuffer, this->gameID, this->relatedPlayer, relatedPlayerLeaderboardId);
	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt(lookupBuffer));
	query->setInt(1, this->gameID);
	if (isFriendsQuery) {
		query->setInt(2, this->relatedPlayer);
		query->setInt(3, this->relatedPlayer);
		query->setInt(4, this->relatedPlayer);
		query->setInt(5, relatedPlayerLeaderboardId);
	} else {
		query->setInt(2, relatedPlayerLeaderboardId);
	}
	std::unique_ptr<sql::ResultSet> result(query->executeQuery());

	if (result->rowsCount() == 0) return;

	uint32_t myRanking = 1;
	uint32_t myCharacterId = 0;
	int32_t lowestRanking = result->rowsCount() - 5;
	uint32_t startRanking = 1; // Default to top 11
	if (this->infoType == InfoType::MyStanding) {
		// Find my ranking in the leaderboard
		while (result->next()) {
			if (result->getInt("character_id") != myCharacterId) myRanking++;
			else break;
		}
		// Once you've found my ranking, figure out if we need to adjust the 
		// row pointer to get the top 11 or the bottom 11.

		if (lowestRanking > 0 && myRanking >= lowestRanking) { // I am in the bottom 10, so set row pointer back to the top of the bottom 6
			for (uint32_t i = myRanking - lowestRanking; i > lowestRanking; i--) {
				result->previous();
			}
		}

		if (myRanking >= 6) startRanking = myRanking - 5; // If i am not in the top 5, set row pointer to 5 above my ranking
		else if (myRanking > result->rowsCount()) { // If i am in the bottom 10, set the row pointer to the top of the bottom 11
			startRanking = result->rowsCount() - 10;
		}

		for (uint32_t i = myRanking - 5; i > 0; i--) { // Adjust first row gotten to be 5 above my ranking.	
			result->previous();
		}
	}

	this->entries.reserve(11);
	for (uint32_t i = 0; i < 11 && result->next(); i++) {
		constexpr int32_t MAX_NUM_DATA_PER_ROW = 9;
		this->entries.push_back(std::vector<LDFBaseData*>());
		auto& entry = this->entries.back();
		entry.reserve(MAX_NUM_DATA_PER_ROW);
		entry.push_back(new LDFData<uint64_t>(u"CharacterID", result->getInt("character_id")));
		entry.push_back(new LDFData<uint64_t>(u"LastPlayed", result->getUInt64("lastPlayed")));
		entry.push_back(new LDFData<int32_t>(u"NumPlayed", 1));
		entry.push_back(new LDFData<std::u16string>(u"name", GeneralUtils::ASCIIToUTF16(result->getString("name").c_str())));
		entry.push_back(new LDFData<int32_t>(u"RowNumber", startRanking + i));
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
			entry.push_back(new LDFData<int32_t>(u"Time", result->getInt("bestTime")));
			// Time:1
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Wave:1
			break;
		case Type::Donations:
			entry.push_back(new LDFData<int32_t>(u"Score", result->getInt("score")));
			// Score:1				
			// Something? idk yet.
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

std::string FormatInsert(const char* columns, const char* format, va_list args) {
	auto queryBase = "INSERT INTO leaderboard (%s) VALUES (%s)";
	constexpr uint16_t STRING_LENGTH = 400;
	char formattedInsert[STRING_LENGTH];
	char finishedQuery[STRING_LENGTH];
	snprintf(formattedInsert, 400, queryBase, columns, format);
	vsnprintf(finishedQuery, 400, formattedInsert, args);
	return finishedQuery;
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, va_list args) {
	std::string insertStatement;
	// use replace into to update the score if it already exists instead of needing an update and an insert
	switch (leaderboardType) {
	case Leaderboard::Type::ShootingGallery: {
		// Check that the score exists and is better. If the score is better update it.
		// If the score is the same but the streak is better, update it.
		// If the score is the same and the streak is the same but the hit percentage is better, update it.
		// If the score doesn't exist, insert it.
		auto lookup = Database::CreatePreppedStmt("SELECT score, streak, hitPercentage FROM leaderboard WHERE playerID = ? AND gameID = ?");
		lookup->setInt64(1, playerID);
		lookup->setInt(2, gameID);
		auto lookupResult = lookup->executeQuery();
		if (lookupResult->next()) {

		} else {
			auto result = FormatInsert("hitPercentage, score, streak", "%f, %i, %i", args);
			Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		}
		break;
	}
	case Leaderboard::Type::Racing: {
		auto result = FormatInsert("bestLapTime, bestTime", "%f, %f", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::UnusedLeaderboard4: {
		auto result = FormatInsert("points", "%i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::MonumentRace: {
		auto result = FormatInsert("time", "%i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::FootRace: {
		auto result = FormatInsert("time", "%i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::Survival: {
		auto result = FormatInsert("points, time", "%i, %i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::SurvivalNS: {
		auto result = FormatInsert("time, wave", "%i, %i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
		break;
	}
	case Leaderboard::Type::Donations: {
		auto result = FormatInsert("score", "%i", args);
		Game::logger->Log("LeaderboardManager", "%s", result.c_str());
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

}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID targetID, LWOOBJID playerID) {
	// Create the leaderboard here and then send it right after.	On the stack.
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
