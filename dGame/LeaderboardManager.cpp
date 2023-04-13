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

#include "CDActivitiesTable.h"

Leaderboard::Leaderboard(const GameID gameID, const Leaderboard::InfoType infoType, const bool weekly, const Leaderboard::Type leaderboardType) {
	this->relatedPlayer = relatedPlayer;
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->entries = entries;
	this->leaderboardType = leaderboardType;
}

bool Leaderboard::IsScoreBetter(const uint32_t score) const {

}

void Leaderboard::Serialize(RakNet::BitStream* bitStream) const {
	std::string leaderboard;

	leaderboard += "ADO.Result=7:1\n";
	leaderboard += "Result.Count=1:1\n";
	leaderboard += "Result[0].Index=0:RowNumber\n";
	leaderboard += "Result[0].RowCount=1:" + std::to_string(entries.size()) + "\n";

	auto index = 0;
	for (const auto& entry : entries) {
		leaderboard += "Result[0].Row[" + std::to_string(index) + "].LastPlayed=8:" + std::to_string(entry.lastPlayed) + "\n";
		leaderboard += "Result[0].Row[" + std::to_string(index) + "].CharacterID=8:" + std::to_string(entry.playerID) + "\n";
		leaderboard += "Result[0].Row[" + std::to_string(index) + "].NumPlayed=1:1\n";
		leaderboard += "Result[0].Row[" + std::to_string(index) + "].RowNumber=8:" + std::to_string(entry.placement) + "\n";
		leaderboard += "Result[0].Row[" + std::to_string(index) + "].Time=1:" + std::to_string(entry.time) + "\n";

		// Only these minigames have a points system
		if (leaderboardType == Survival || leaderboardType == ShootingGallery) {
			leaderboard += "Result[0].Row[" + std::to_string(index) + "].Points=1:" + std::to_string(entry.score) + "\n";
		} else if (leaderboardType == SurvivalNS) {
			leaderboard += "Result[0].Row[" + std::to_string(index) + "].Wave=1:" + std::to_string(entry.score) + "\n";
		}

		leaderboard += "Result[0].Row[" + std::to_string(index) + "].name=0:" + entry.playerName + "\n";
		index++;
	}

	// Serialize the thing to a BitStream
	bitStream->WriteAlignedBytes((const unsigned char*)leaderboard.c_str(), leaderboard.size());
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
