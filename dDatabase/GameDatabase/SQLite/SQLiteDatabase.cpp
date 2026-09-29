#include "SQLiteDatabase.h"

#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "dPlatforms.h"
#include "BinaryPathFinder.h"

void SQLiteDatabase::Connect() {
	LOG("Using SQLite database");
	m_Con = new CppSQLite3DB();
	const auto path = BinaryPathFinder::GetBinaryDir() / Game::config->GetValue("sqlite_database_path");

	if (!std::filesystem::exists(path)) {
		LOG("Creating sqlite path %s", path.string().c_str());
		std::filesystem::create_directories(path.parent_path());
	}

	m_Con->open(path.string().c_str());

	// Make sure wal is enabled for the database.
	m_Con->execQuery("PRAGMA journal_mode = WAL;");
}

void SQLiteDatabase::Destroy(std::string source) {
	if (!m_Con) return;

	if (source.empty()) LOG("Destroying SQLite connection!");
	else LOG("Destroying SQLite connection from %s!", source.c_str());

	m_Con->close();
	delete m_Con;
	m_Con = nullptr;
}

void SQLiteDatabase::ExecuteCustomQuery(const std::string_view query) {
	m_Con->compileStatement(query.data()).execDML();
}

CppSQLite3Statement SQLiteDatabase::CreatePreppedStmt(const std::string& query) {
	return m_Con->compileStatement(query.c_str());
}

void SQLiteDatabase::Commit() {
	if (!m_Con->IsAutoCommitOn()) m_Con->compileStatement("COMMIT;").execDML();
}

bool SQLiteDatabase::GetAutoCommit() {
	return m_Con->IsAutoCommitOn();
}

void SQLiteDatabase::SetAutoCommit(bool value) {
	if (value) {
		if (!GetAutoCommit()) m_Con->compileStatement("COMMIT;").execDML();
	} else {
		if (GetAutoCommit()) m_Con->compileStatement("BEGIN;").execDML();
	}
}

void SQLiteDatabase::Rollback() {
	// A failed statement can already have ended the transaction; only roll back one that is open
	if (!m_Con->IsAutoCommitOn()) m_Con->compileStatement("ROLLBACK;").execDML();
}

void SQLiteDatabase::DeleteCharacter(const LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM charxml WHERE id=?;", characterId);
	ExecuteDelete("DELETE FROM command_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM friends WHERE player_id=? OR friend_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM leaderboard WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM properties_contents WHERE property_id IN (SELECT id FROM properties WHERE owner_id=?);", characterId);
	ExecuteDelete("DELETE FROM properties WHERE owner_id=?;", characterId);
	ExecuteDelete("DELETE FROM ugc WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM activity_log WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM mail WHERE receiver_id=?;", characterId);
	ExecuteDelete("DELETE FROM ignore_list WHERE player_id=? OR ignored_player_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM pet_names WHERE owner_id=?;", characterId);
	ExecuteDelete("DELETE FROM player_positions WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM guild_members WHERE character_id=?;", characterId);
	ExecuteDelete("DELETE FROM guild_invites WHERE character_id=? OR inviter_id=?;", characterId, characterId);
	ExecuteDelete("DELETE FROM charinfo WHERE id=?;", characterId);
}
