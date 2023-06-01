#include "LeaderboardManager.h"

#include <sstream>
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
#include "DluAssert.h"

#include "CDActivitiesTable.h"
#include "Metrics.hpp"

namespace LeaderboardManager {
	LeaderboardCache leaderboardCache;
}

Leaderboard::Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, LWOOBJID relatedPlayer, const Leaderboard::Type leaderboardType) {
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->leaderboardType = leaderboardType;
	this->relatedPlayer = relatedPlayer;
}

Leaderboard::~Leaderboard() {
	Clear();
}

void Leaderboard::Clear() {
	for (auto& entry : entries) for (auto data : entry) delete data;

}

inline void WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, LDFBaseData* data) {
	leaderboard << "\nResult[0].Row[" << index << "]." << data->GetString();
}

void Leaderboard::Serialize(RakNet::BitStream* bitStream) const {
	bitStream->Write(gameID);
	bitStream->Write(infoType);

	std::ostringstream leaderboard;

	leaderboard << "ADO.Result=7:1"; // Unused in 1.10.64, but is in captures
	leaderboard << "\nResult.Count=1:1"; // number of results, always 1
	if (!this->entries.empty()) leaderboard << "\nResult[0].Index=0:RowNumber"; // "Primary key".  Live doesn't include this if there are no entries.
	leaderboard << "\nResult[0].RowCount=1:" << entries.size();

	int32_t rowNumber = 0;
	for (auto& entry : entries) {
		for (auto* data : entry) {
			WriteLeaderboardRow(leaderboard, rowNumber, data);
		}
		rowNumber++;
	}

	// Serialize the thing to a BitStream
	uint32_t leaderboardSize = leaderboard.tellp();
	bitStream->Write<uint32_t>(leaderboardSize);
	// Doing this all in 1 call so there is no possbility of a dangling pointer.
	bitStream->WriteAlignedBytes(reinterpret_cast<const unsigned char*>(GeneralUtils::ASCIIToUTF16(leaderboard.str()).c_str()), leaderboardSize * sizeof(char16_t));
	bitStream->Write0();
	bitStream->Write0();
}

void Leaderboard::QueryToLdf(std::unique_ptr<sql::ResultSet>& rows) {
	Clear();
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
		entry.push_back(new LDFData<uint64_t>(u"RowNumber", rows->getInt("ranking")));
		switch (leaderboardType) {
		case Type::ShootingGallery:
			entry.push_back(new LDFData<float>(u"HitPercentage", (rows->getInt("primaryScore") / 100.0f)));
			// HitPercentage:3 between 0 and 1
			entry.push_back(new LDFData<int32_t>(u"Score", rows->getInt("secondaryScore")));
			// Score:1
			entry.push_back(new LDFData<int32_t>(u"Streak", rows->getInt("tertiaryScore")));
			// Streak:1
			break;
		case Type::Racing:
			entry.push_back(new LDFData<float>(u"BestTime", rows->getDouble("primaryScore")));
			// BestLapTime:3
			entry.push_back(new LDFData<float>(u"BestLapTime", rows->getDouble("secondaryScore")));
			// BestTime:3
			entry.push_back(new LDFData<int32_t>(u"License", 1));
			// License:1 - 1 if player has completed mission 637 and 0 otherwise
			entry.push_back(new LDFData<int32_t>(u"NumWins", rows->getInt("tertiaryScore")));
			// NumWins:1
			break;
		case Type::UnusedLeaderboard4:
			entry.push_back(new LDFData<int32_t>(u"Points", rows->getInt("primaryScore")));
			// Points:1
			break;
		case Type::MonumentRace:
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("primaryScore")));
			// Time:1(?)
			break;
		case Type::FootRace:
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("primaryScore")));
			// Time:1
			break;
		case Type::Survival:
			entry.push_back(new LDFData<int32_t>(u"Points", rows->getInt("primaryScore")));
			// Points:1
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("secondaryScore")));
			// Time:1
			break;
		case Type::SurvivalNS:
			entry.push_back(new LDFData<int32_t>(u"Wave", rows->getInt("primaryScore")));
			// Wave:1
			entry.push_back(new LDFData<int32_t>(u"Time", rows->getInt("secondaryScore")));
			// Time:1
			break;
		case Type::Donations:
			entry.push_back(new LDFData<int32_t>(u"Points", rows->getInt("primaryScore")));
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

const std::string_view Leaderboard::GetOrdering(Leaderboard::Type leaderboardType) {
	// Use a switch case and return desc for all 3 columns if higher is better and asc if lower is better
	switch (leaderboardType) {
	case Type::ShootingGallery:
	case Type::FootRace:
	case Type::UnusedLeaderboard4:
	case Type::SurvivalNS:
	case Type::Donations:
		return "primaryScore DESC, secondaryScore DESC, tertiaryScore DESC";
	case Type::Racing:
	case Type::MonumentRace:
		return "primaryScore ASC, secondaryScore ASC, tertiaryScore ASC";
	case Type::None:
	case Type::Survival:
		return Game::config->GetValue("classic_survival_scoring") == "1" ?
			"primaryScore DESC, secondaryScore DESC, tertiaryScore DESC" :
			"secondaryScore DESC, primaryScore DESC, tertiaryScore DESC";
	default:
		return "";
	}
}

void Leaderboard::SetupLeaderboard(uint32_t resultStart, uint32_t resultEnd) {
	resultStart++;
	resultEnd++;
	const std::string queryBase =
		R"QUERY( 
		WITH leaderboardsRanked AS ( 
			SELECT leaderboard.primaryScore, leaderboard.secondaryScore, leaderboard.tertiaryScore, charinfo.name, 
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
		SELECT leaderboardsRanked.*, character_id, UNIX_TIMESTAMP(last_played) as lastPlayed, leaderboardsRanked.name, leaderboardsRanked.ranking FROM leaderboardsRanked, myStanding, lowestRanking 
		WHERE leaderboardsRanked.ranking 
		BETWEEN 
		LEAST(GREATEST(CAST(myRank AS SIGNED) - 5, %i), lowestRanking.lowestRank - 10) 
		AND 
		LEAST(GREATEST(myRank + 5, %i), lowestRanking.lowestRank) 
		ORDER BY ranking ASC;
	)QUERY";

	// If we are getting the friends leaderboard, add the friends query, otherwise fill it in with nothing.
	std::string friendsQuery =
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

	if (this->infoType != InfoType::Friends) friendsQuery.clear();
	const auto orderBase = GetOrdering(this->leaderboardType);

	std::string baseLookup;
	if (this->infoType == InfoType::Top) {
		baseLookup = "SELECT id FROM leaderboard WHERE game_id = ? ORDER BY ";
		baseLookup += orderBase.data();
	} else {
		baseLookup = "SELECT id FROM leaderboard WHERE game_id = ? AND character_id = ";
		baseLookup += std::to_string(this->relatedPlayer);
	}
	baseLookup += " LIMIT 1";

	std::unique_ptr<sql::PreparedStatement> baseQuery(Database::CreatePreppedStmt(baseLookup));
	baseQuery->setInt(1, this->gameID);
	std::unique_ptr<sql::ResultSet> baseResult(baseQuery->executeQuery());

	if (!baseResult->next()) return; // In this case, there are no entries in the leaderboard for this game.

	uint32_t relatedPlayerLeaderboardId = baseResult->getInt("id");

	// Create and execute the actual save here
	constexpr uint16_t STRING_LENGTH = 2048;
	char lookupBuffer[STRING_LENGTH];
	[[maybe_unused]] int32_t res = snprintf(lookupBuffer, STRING_LENGTH, queryBase.data(), orderBase.data(), friendsQuery.data(), resultStart, resultEnd);
	DluAssert(res != -1);
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

void Leaderboard::Send(const LWOOBJID targetID) const {
	auto* player = EntityManager::Instance()->GetEntity(relatedPlayer);
	if (player != nullptr) {
		GameMessages::SendActivitySummaryLeaderboardData(targetID, this, player->GetSystemAddress());
	}
}

std::string FormatInsert(const Leaderboard::Type& type, const Score& score, const bool useUpdate) {
	std::string insertStatement;
	if (useUpdate) {
		insertStatement =
			R"QUERY(
			UPDATE leaderboard 
			SET primaryScore %f, secondaryScore %f, tertiaryScore %f, 
			timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;
			)QUERY";
	} else {
		insertStatement =
			R"QUERY(
			INSERT leaderboard SET 
			primaryScore %f, secondaryScore %f, tertiaryScore %f, 
			character_id = ?, game_id = ?;
			)QUERY";
	}

	constexpr uint16_t STRING_LENGTH = 400;
	// Then fill in our score
	char finishedQuery[STRING_LENGTH];
	int32_t res = snprintf(finishedQuery, STRING_LENGTH, insertStatement.c_str(), score.GetPrimaryScore(), score.GetSecondaryScore(), score.GetTertiaryScore());
	DluAssert(res != -1);
	return finishedQuery;
}

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, const GameID gameID, const Leaderboard::Type leaderboardType, const float primaryScore, const float secondaryScore, const float tertiaryScore) {
	auto* lookup = "SELECT * FROM leaderboard WHERE character_id = ? AND game_id = ?;";

	std::unique_ptr<sql::PreparedStatement> query(Database::CreatePreppedStmt(lookup));
	query->setInt(1, playerID);
	query->setInt(2, gameID);
	std::unique_ptr<sql::ResultSet> myScoreResult(query->executeQuery());

	std::string saveQuery("UPDATE leaderboard SET timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;");
	Score newScore(primaryScore, secondaryScore, tertiaryScore);
	if (myScoreResult->next()) {
		Score oldScore;
		bool lowerScoreBetter = false;
		switch (leaderboardType) {
			// Higher score better
		case Leaderboard::Type::ShootingGallery: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			oldScore.SetSecondaryScore(myScoreResult->getInt("secondaryScore"));
			oldScore.SetTertiaryScore(myScoreResult->getInt("tertiaryScore"));
			break;
		}
		case Leaderboard::Type::FootRace: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			break;
		}
		case Leaderboard::Type::Survival: {
			// Config option may reverse these
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			oldScore.SetSecondaryScore(myScoreResult->getInt("secondaryScore"));
			break;
		}
		case Leaderboard::Type::SurvivalNS: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			oldScore.SetSecondaryScore(myScoreResult->getInt("secondaryScore"));
			break;
		}
		case Leaderboard::Type::UnusedLeaderboard4:
		case Leaderboard::Type::Donations: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			break;
		}
		case Leaderboard::Type::Racing: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			oldScore.SetSecondaryScore(myScoreResult->getInt("secondaryScore"));
			lowerScoreBetter = true;
			break;
		}
		case Leaderboard::Type::MonumentRace: {
			oldScore.SetPrimaryScore(myScoreResult->getInt("primaryScore"));
			lowerScoreBetter = true;
			// Do score checking here
			break;
		}
		case Leaderboard::Type::None:
		default:
			Game::logger->Log("LeaderboardManager", "Unknown leaderboard type %i for game %i.  Cannot save score!", leaderboardType, gameID);
			return;
		}
		bool newHighScore = lowerScoreBetter ? newScore < oldScore : newScore > oldScore;
		if (newHighScore) {
			saveQuery = FormatInsert(leaderboardType, newScore, true);
		} else if (leaderboardType == Leaderboard::Type::Racing && tertiaryScore) {
			saveQuery = "UPDATE leaderboard SET numWins = numWins + 1, timesPlayed = timesPlayed + 1 WHERE character_id = ? AND game_id = ?;";
		}
	} else {
		saveQuery = FormatInsert(leaderboardType, newScore, false);
	}

	std::unique_ptr<sql::PreparedStatement> saveStatement(Database::CreatePreppedStmt(saveQuery));
	saveStatement->setInt(1, playerID);
	saveStatement->setInt(2, gameID);
	saveStatement->execute();
}

void LeaderboardManager::SendLeaderboard(const uint32_t gameID, const Leaderboard::InfoType infoType, const bool weekly, const LWOOBJID playerID, const LWOOBJID targetID, const uint32_t resultStart, const uint32_t resultEnd) {
	Leaderboard leaderboard(gameID, infoType, weekly, playerID, GetLeaderboardType(gameID));
	leaderboard.SetupLeaderboard(resultStart, resultEnd);
	leaderboard.Send(targetID);
}

Leaderboard::Type LeaderboardManager::GetLeaderboardType(const GameID gameID) {
	auto lookup = leaderboardCache.find(gameID);
	if (lookup != leaderboardCache.end()) return lookup->second;

	auto* activitiesTable = CDClientManager::Instance().GetTable<CDActivitiesTable>();
	std::vector<CDActivities> activities = activitiesTable->Query([=](const CDActivities& entry) {
		return entry.ActivityID == gameID;
		});
	auto type = !activities.empty() ? static_cast<Leaderboard::Type>(activities.at(0).leaderboardType) : Leaderboard::Type::None;
	leaderboardCache.insert_or_assign(gameID, type);
	return type;
}
