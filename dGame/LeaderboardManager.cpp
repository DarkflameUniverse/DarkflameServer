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
Leaderboard::Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const Leaderboard::Type leaderboardType) {
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->leaderboardType = leaderboardType;
}

template<class TypeToWrite>
void Leaderboard::WriteLeaderboardRow(std::ostringstream& leaderboard, const uint32_t& index, const std::string& key, const eLDFType& ldfType, const TypeToWrite& value) const {
	leaderboard << "Result[0].Row[" << index << "]." << key << '=' << ldfType << ':' << value << '\n';
}

void Leaderboard::Serialize(RakNet::BitStream* bitStream) const {
	std::ostringstream leaderboard;

	leaderboard << "ADO.Result=7:1\n"; // Unused in 1.10.64, but is in captures
	leaderboard << "Result.Count=1:1\n"; // number of results, always 1?
	leaderboard << "Result[0].Index=0:RowNumber\n"; // "Primary key"
	leaderboard << "Result[0].RowCount=1:" << entries.size() << '\n'; // number of rows

	auto index = 0;
	for (const auto& entry : entries) {
		WriteLeaderboardRow(leaderboard, index, "CharacterID", eLDFType::LDF_TYPE_U64, entry.playerID);
		WriteLeaderboardRow(leaderboard, index, "LastPlayed", eLDFType::LDF_TYPE_U64, entry.lastPlayed);
		WriteLeaderboardRow(leaderboard, index, "NumPlayed", eLDFType::LDF_TYPE_S32, 1);
		WriteLeaderboardRow(leaderboard, index, "name", eLDFType::LDF_TYPE_UTF_16, entry.playerName);
		WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_S32, entry.placement);

		// Each minigame has its own "points" system
		switch (leaderboardType) {
			case Type::ShootingGallery:
				WriteLeaderboardRow(leaderboard, index, "HitPercentage", eLDFType::LDF_TYPE_FLOAT, 0.0f);
				// HitPercentage:3 between 0 and 1
				WriteLeaderboardRow(leaderboard, index, "Score", eLDFType::LDF_TYPE_S32, entry.score);
				// Score:1
				WriteLeaderboardRow(leaderboard, index, "Streak", eLDFType::LDF_TYPE_S32, 0);
				// Streak:1
				break;
			case Type::Racing:
				WriteLeaderboardRow(leaderboard, index, "BestLapTime", eLDFType::LDF_TYPE_FLOAT, 0.0f);
				// BestLapTime:3
				WriteLeaderboardRow(leaderboard, index, "BestTime", eLDFType::LDF_TYPE_FLOAT, 0.0f);
				// BestTime:3
				WriteLeaderboardRow(leaderboard, index, "License", eLDFType::LDF_TYPE_S32, 0);
				// License:1 - 1 if player has completed mission 637 and 0 otherwise
				WriteLeaderboardRow(leaderboard, index, "NumWins", eLDFType::LDF_TYPE_S32, 0);
				// NumWins:1
				break;
			case Type::UnusedLeaderboard4:
				WriteLeaderboardRow(leaderboard, index, "Points", eLDFType::LDF_TYPE_S32, entry.score);
				// Points:1
				break;
			case Type::MonumentRace:
				WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, entry.time);
				// Time:1(?)
				break;
			case Type::FootRace:
				WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, entry.time);
				// Time:1
				break;
			case Type::Survival:
				WriteLeaderboardRow(leaderboard, index, "Points", eLDFType::LDF_TYPE_S32, entry.score);
				// Points:1
				WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, entry.time);
				// Time:1
				break;
			case Type::SurvivalNS:
				WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, entry.time);
				// Time:1
				WriteLeaderboardRow(leaderboard, index, "Wave", eLDFType::LDF_TYPE_S32, entry.score);
				// Wave:1
				break;
			case Type::Donations:
				WriteLeaderboardRow(leaderboard, index, "Score", eLDFType::LDF_TYPE_S32, entry.score);
				// Score:1				
				// Something? idk yet.
				break;
			case Type::None:
				// This type is included here simply to resolve a compiler warning on mac about unused enum types
				break;
			default:
				break;
		}
		index++;
	}

	// Serialize the thing to a BitStream
	bitStream->Write(leaderboard.str().c_str(), leaderboard.tellp());
}

void Leaderboard::SetupLeaderboard() {
	// Setup query based on activity. 
	// Where clause will vary based on what query we are doing
	
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

void LeaderboardManager::SaveScore(const LWOOBJID& playerID, GameID gameID, Leaderboard::Type leaderboardType, va_list args){
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
			Game::logger->Log("LeaderboardManager", "Unknown leaderboard type %i.  Cannot save score!", leaderboardType);
			return;
		}
	}

}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID targetID, LWOOBJID playerID) {
	// Create the leaderboard here and then send it right after.  On the stack.
	Leaderboard leaderboard(gameID, infoType, weekly, GetLeaderboardType(gameID));
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
