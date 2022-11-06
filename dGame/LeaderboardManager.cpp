#include "LeaderboardManager.h"
#include <utility>
#include "Database.h"
#include "EntityManager.h"
#include "Character.h"
#include "Game.h"
#include "GameMessages.h"
#include "dLogger.h"
#include "dConfig.h"

Leaderboard::Leaderboard(uint32_t gameID, uint32_t infoType, bool weekly, std::vector<LeaderboardEntry> entries,
	LWOOBJID relatedPlayer, LeaderboardType leaderboardType) {
	this->relatedPlayer = relatedPlayer;
	this->gameID = gameID;
	this->weekly = weekly;
	this->infoType = infoType;
	this->entries = std::move(entries);
	this->leaderboardType = leaderboardType;
}

std::u16string Leaderboard::ToString() const {
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

	return GeneralUtils::UTF8ToUTF16(leaderboard);
}

std::vector<LeaderboardEntry> Leaderboard::GetEntries() {
	return entries;
}

uint32_t Leaderboard::GetGameID() const {
	return gameID;
}

uint32_t Leaderboard::GetInfoType() const {
	return infoType;
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
	if (character == nullptr)
		return;

	auto* select = Database::CreatePreppedStmt("SELECT time, score FROM leaderboard WHERE character_id = ? AND game_id = ?;");

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
		case ShootingGallery:
			if (score <= storedScore)
				highscore = false;
			break;
		case Racing:
			if (time >= storedTime)
				highscore = false;
			break;
		case MonumentRace:
			if (time >= storedTime)
				highscore = false;
			break;
		case FootRace:
			if (time <= storedTime)
				highscore = false;
			break;
		case Survival:
			if (classicSurvivalScoring) {
				if (time <= storedTime) { // Based on time (LU live)
					highscore = false;
				}
			} else {
				if (score <= storedScore) // Based on score (DLU)
					highscore = false;
			}
			break;
		case SurvivalNS:
			if (!(score > storedScore || (time < storedTime && score >= storedScore)))
				highscore = false;
			break;
		default:
			highscore = false;
		}

		if (!highscore) {
			delete select;
			delete result;
			return;
		}
	}

	delete select;
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

Leaderboard* LeaderboardManager::GetLeaderboard(uint32_t gameID, InfoType infoType, bool weekly, LWOOBJID playerID) {
	auto leaderboardType = GetLeaderboardType(gameID);

	std::string query;
	bool classicSurvivalScoring = Game::config->GetValue("classic_survival_scoring") == "1";
	switch (infoType) {
	case InfoType::Standings:
		switch (leaderboardType) {
		case ShootingGallery:
			query = standingsScoreQuery; // Shooting gallery is based on the highest score.
			break;
		case FootRace:
			query = standingsTimeQuery; // The higher your time, the better for FootRace.
			break;
		case Survival:
			query = classicSurvivalScoring ? standingsTimeQuery : standingsScoreQuery;
			break;
		case SurvivalNS:
			query = standingsScoreQueryAsc; // BoNS is scored by highest wave (score) first, then time.
			break;
		default:
			query = standingsTimeQueryAsc; // MonumentRace and Racing are based on the shortest time.
		}
		break;
	case InfoType::Friends:
		switch (leaderboardType) {
		case ShootingGallery:
			query = friendsScoreQuery; // Shooting gallery is based on the highest score.
			break;
		case FootRace:
			query = friendsTimeQuery; // The higher your time, the better for FootRace.
			break;
		case Survival:
			query = classicSurvivalScoring ? friendsTimeQuery : friendsScoreQuery;
			break;
		case SurvivalNS:
			query = friendsScoreQueryAsc; // BoNS is scored by highest wave (score) first, then time.
			break;
		default:
			query = friendsTimeQueryAsc;  // MonumentRace and Racing are based on the shortest time.
		}
		break;

	default:
		switch (leaderboardType) {
		case ShootingGallery:
			query = topPlayersScoreQuery; // Shooting gallery is based on the highest score.
			break;
		case FootRace:
			query = topPlayersTimeQuery; // The higher your time, the better for FootRace.
			break;
		case Survival:
			query = classicSurvivalScoring ? topPlayersTimeQuery : topPlayersScoreQuery;
			break;
		case SurvivalNS:
			query = topPlayersScoreQueryAsc; // BoNS is scored by highest wave (score) first, then time.
			break;
		default:
			query = topPlayersTimeQueryAsc;  // MonumentRace and Racing are based on the shortest time.
		}
	}

	auto* statement = Database::CreatePreppedStmt(query);
	statement->setUInt(1, gameID);

	// Only the standings and friends leaderboards require the character ID to be set
	if (infoType == Standings || infoType == Friends) {
		auto characterID = 0;

		const auto* player = EntityManager::Instance()->GetEntity(playerID);
		if (player != nullptr) {
			auto* character = player->GetCharacter();
			if (character != nullptr)
				characterID = character->GetID();
		}

		statement->setUInt64(2, characterID);
	}

	auto* res = statement->executeQuery();

	std::vector<LeaderboardEntry> entries{};

	uint32_t index = 0;
	while (res->next()) {
		LeaderboardEntry entry;
		entry.playerID = res->getUInt64(4);
		entry.playerName = res->getString(5);
		entry.time = res->getUInt(1);
		entry.score = res->getUInt(2);
		entry.placement = res->getUInt(3);
		entry.lastPlayed = res->getUInt(6);

		entries.push_back(entry);
		index++;
	}

	delete res;
	delete statement;

	return new Leaderboard(gameID, infoType, weekly, entries, playerID, leaderboardType);
}

void LeaderboardManager::SendLeaderboard(uint32_t gameID, InfoType infoType, bool weekly, LWOOBJID targetID,
	LWOOBJID playerID) {
	const auto* leaderboard = LeaderboardManager::GetLeaderboard(gameID, infoType, weekly, playerID);
	leaderboard->Send(targetID);
	delete leaderboard;
}

LeaderboardType LeaderboardManager::GetLeaderboardType(uint32_t gameID) {
	auto* activitiesTable = CDClientManager::Instance()->GetTable<CDActivitiesTable>("Activities");
	std::vector<CDActivities> activities = activitiesTable->Query([=](const CDActivities& entry) {
		return (entry.ActivityID == gameID);
		});

	for (const auto& activity : activities) {
		return static_cast<LeaderboardType>(activity.leaderboardType);
	}

	return LeaderboardType::None;
}

const std::string LeaderboardManager::topPlayersScoreQuery =
"WITH leaderboard_vales AS ( "
"        SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"RANK() OVER ( ORDER BY l.score DESC, l.time DESC, last_played ) leaderboard_rank "
"        FROM leaderboard l "
"INNER JOIN charinfo c ON l.character_id = c.id "
"WHERE l.game_id = ? "
"ORDER BY leaderboard_rank) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales LIMIT 11;";

const std::string LeaderboardManager::friendsScoreQuery =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, f.friend_id, f.player_id, "
"           RANK() OVER ( ORDER BY l.score DESC, l.time DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"        INNER JOIN friends f ON f.player_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"     personal_values AS ( "
"         SELECT id as related_player_id, "
"                GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"                GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"         FROM leaderboard_vales WHERE leaderboard_vales.id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank AND (player_id = related_player_id OR friend_id = related_player_id);";

const std::string LeaderboardManager::standingsScoreQuery =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"           RANK() OVER ( ORDER BY l.score DESC, l.time DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"personal_values AS ( "
"    SELECT GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"           GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"    FROM leaderboard_vales WHERE id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank;";

const std::string LeaderboardManager::topPlayersScoreQueryAsc =
"WITH leaderboard_vales AS ( "
"        SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"RANK() OVER ( ORDER BY l.score DESC, l.time ASC, last_played ) leaderboard_rank "
"        FROM leaderboard l "
"INNER JOIN charinfo c ON l.character_id = c.id "
"WHERE l.game_id = ? "
"ORDER BY leaderboard_rank) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales LIMIT 11;";

const std::string LeaderboardManager::friendsScoreQueryAsc =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, f.friend_id, f.player_id, "
"           RANK() OVER ( ORDER BY l.score DESC, l.time ASC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"        INNER JOIN friends f ON f.player_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"     personal_values AS ( "
"         SELECT id as related_player_id, "
"                GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"                GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"         FROM leaderboard_vales WHERE leaderboard_vales.id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank AND (player_id = related_player_id OR friend_id = related_player_id);";

const std::string LeaderboardManager::standingsScoreQueryAsc =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"           RANK() OVER ( ORDER BY l.score DESC, l.time ASC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"personal_values AS ( "
"    SELECT GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"           GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"    FROM leaderboard_vales WHERE id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank;";

const std::string LeaderboardManager::topPlayersTimeQuery =
"WITH leaderboard_vales AS ( "
"        SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"RANK() OVER ( ORDER BY l.time DESC, l.score DESC, last_played ) leaderboard_rank "
"        FROM leaderboard l "
"INNER JOIN charinfo c ON l.character_id = c.id "
"WHERE l.game_id = ? "
"ORDER BY leaderboard_rank) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales LIMIT 11;";

const std::string LeaderboardManager::friendsTimeQuery =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, f.friend_id, f.player_id, "
"           RANK() OVER ( ORDER BY l.time DESC, l.score DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"        INNER JOIN friends f ON f.player_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"     personal_values AS ( "
"         SELECT id as related_player_id, "
"                GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"                GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"         FROM leaderboard_vales WHERE leaderboard_vales.id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank AND (player_id = related_player_id OR friend_id = related_player_id);";

const std::string LeaderboardManager::standingsTimeQuery =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"           RANK() OVER ( ORDER BY l.time DESC, l.score DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"personal_values AS ( "
"    SELECT GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"           GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"    FROM leaderboard_vales WHERE id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank;";

const std::string LeaderboardManager::topPlayersTimeQueryAsc =
"WITH leaderboard_vales AS ( "
"        SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"RANK() OVER ( ORDER BY l.time ASC, l.score DESC, last_played ) leaderboard_rank "
"        FROM leaderboard l "
"INNER JOIN charinfo c ON l.character_id = c.id "
"WHERE l.game_id = ? "
"ORDER BY leaderboard_rank) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales LIMIT 11;";

const std::string LeaderboardManager::friendsTimeQueryAsc =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, f.friend_id, f.player_id, "
"           RANK() OVER ( ORDER BY l.time ASC, l.score DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"        INNER JOIN friends f ON f.player_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"     personal_values AS ( "
"         SELECT id as related_player_id, "
"                GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"                GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"         FROM leaderboard_vales WHERE leaderboard_vales.id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank AND (player_id = related_player_id OR friend_id = related_player_id);";

const std::string LeaderboardManager::standingsTimeQueryAsc =
"WITH leaderboard_vales AS ( "
"    SELECT l.time, l.score, UNIX_TIMESTAMP(l.last_played) last_played, c.name, c.id, "
"           RANK() OVER ( ORDER BY l.time ASC, l.score DESC, last_played ) leaderboard_rank "
"    FROM leaderboard l "
"        INNER JOIN charinfo c ON l.character_id = c.id "
"    WHERE l.game_id = ? "
"    ORDER BY leaderboard_rank), "
"personal_values AS ( "
"    SELECT GREATEST(CAST(leaderboard_rank AS SIGNED) - 5, 1) AS min_rank, "
"           GREATEST(leaderboard_rank + 5, 11) AS max_rank "
"    FROM leaderboard_vales WHERE id = ? LIMIT 1) "
"SELECT time, score, leaderboard_rank, id, name, last_played "
"FROM leaderboard_vales, personal_values "
"WHERE leaderboard_rank BETWEEN min_rank AND max_rank;";
