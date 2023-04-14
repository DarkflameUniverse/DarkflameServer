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

		// Each minigame has its own "points" system
		switch (leaderboardType) {
			case Type::ShootingGallery:
			WriteLeaderboardRow(leaderboard, index, "HitPercentage", eLDFType::LDF_TYPE_FLOAT, 0.0f);
			// HitPercentage:3 between 0 and 1
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_S32, entry.placement);
			// RowNumber:1
			WriteLeaderboardRow(leaderboard, index, "Score", eLDFType::LDF_TYPE_S32, entry.score);
			// Score:1
			case Type::Racing:
			WriteLeaderboardRow(leaderboard, index, "BestLapTime", eLDFType::LDF_TYPE_FLOAT, 0.0f);
			// BestLapTime:3
			WriteLeaderboardRow(leaderboard, index, "BestTime", eLDFType::LDF_TYPE_FLOAT, 0.0f);
			// BestTime:3
			WriteLeaderboardRow(leaderboard, index, "License", eLDFType::LDF_TYPE_S32, 0);
			// License:1
			WriteLeaderboardRow(leaderboard, index, "NumWins", eLDFType::LDF_TYPE_S32, 0);
			// NumWins:1
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_U64, entry.placement);
			// RowNumber:8
			case Type::MonumentRace:
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_S32, entry.placement);
			// RowNumber:1
			// Time:1(?)
			case Type::FootRace:
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_S32, entry.placement);
			// RowNumber:1
			WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, 0);
			// Time:1
			case Type::Survival:
			WriteLeaderboardRow(leaderboard, index, "Points", eLDFType::LDF_TYPE_S32, entry.score);
			// Points:1
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_S32, entry.placement);
			// RowNumber:1
			WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, 0);
			// Time:1
			case Type::SurvivalNS:
			WriteLeaderboardRow(leaderboard, index, "RowNumber", eLDFType::LDF_TYPE_U64, entry.placement);
			// RowNumber:8
			WriteLeaderboardRow(leaderboard, index, "Time", eLDFType::LDF_TYPE_S32, entry.time);
			// Time:1
			WriteLeaderboardRow(leaderboard, index, "Wave", eLDFType::LDF_TYPE_S32, entry.score);
			// Wave:1
			case Type::Donations:
			// Something? idk yet.
			case Type::None:
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

void LeaderboardManager::SaveScore(LWOOBJID playerID, uint32_t gameID, uint32_t score, uint32_t time) {
	const auto* player = EntityManager::Instance()->GetEntity(playerID);
	if (player == nullptr)
		return;

	auto* character = player->GetCharacter();
	if (!character)
		return;

	std::unique_ptr<sql::PreparedStatement> select(Database::CreatePreppedStmt("SELECT time, score FROM leaderboard WHERE character_id = ? AND game_id = ?;"));

	select->setUInt64(1, character->GetID());
	select->setInt(2, gameID);

	auto any = false;
	auto* result = select->executeQuery();
	auto leaderboardType = GetLeaderboardType(gameID);

	// Check if the new score is a high score
	while (result->next()) {
		any = true;

		const auto storedTime = result->getInt(1);
		const auto storedScore = result->getInt(2);
		auto highscore = true;
		bool classicSurvivalScoring = Game::config->GetValue("classic_survival_scoring") == "1";

		switch (leaderboardType) {
		case Leaderboard::Type::ShootingGallery:
			if (score <= storedScore)
				highscore = false;
			break;
		case Leaderboard::Type::Racing:
			if (time >= storedTime)
				highscore = false;
			break;
		case Leaderboard::Type::MonumentRace:
			if (time >= storedTime)
				highscore = false;
			break;
		case Leaderboard::Type::FootRace:
			if (time <= storedTime)
				highscore = false;
			break;
		case Leaderboard::Type::Survival:
			if (classicSurvivalScoring) {
				if (time <= storedTime) { // Based on time (LU live)
					highscore = false;
				}
			} else {
				if (score <= storedScore) // Based on score (DLU)
					highscore = false;
			}
			break;
		case Leaderboard::Type::SurvivalNS:
			if (!(score > storedScore || (time < storedTime && score >= storedScore)))
				highscore = false;
			break;
		default:
			highscore = false;
		}

		if (!highscore) {
			delete result;
			return;
		}
	}

	delete result;

	if (any) {
		auto* statement = Database::CreatePreppedStmt("UPDATE leaderboard SET time = ?, score = ?, last_played=SYSDATE() WHERE character_id = ? AND game_id = ?;");
		statement->setInt(1, time);
		statement->setInt(2, score);
		statement->setUInt64(3, character->GetID());
		statement->setInt(4, gameID);
		statement->execute();

		delete statement;
	} else {
		// Note: last_played will be set to SYSDATE() by default when inserting into leaderboard
		auto* statement = Database::CreatePreppedStmt("INSERT INTO leaderboard (character_id, game_id, time, score) VALUES (?, ?, ?, ?);");
		statement->setUInt64(1, character->GetID());
		statement->setInt(2, gameID);
		statement->setInt(3, time);
		statement->setInt(4, score);
		statement->execute();

		delete statement;
	}
}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, Leaderboard::InfoType infoType, bool weekly, LWOOBJID targetID,
	LWOOBJID playerID) {
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
