#include "SQLiteDatabase.h"

#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "dPlatforms.h"
#include "BinaryPathFinder.h"

// Static Variables

// Status Variables
namespace {
	CppSQLite3DB* con = nullptr;
	bool isConnected = false;
};

void SQLiteDatabase::Connect() {
	LOG("Using SQLite database");
	con = new CppSQLite3DB();
	const auto path = BinaryPathFinder::GetBinaryDir() / Game::config->GetValue("sqlite_database_path");

	if (!std::filesystem::exists(path)) {
		LOG("Creating sqlite path %s", path.string().c_str());
		std::filesystem::create_directories(path.parent_path());
	}

	con->open(path.string().c_str());
	isConnected = true;

	// Make sure wal is enabled for the database.
	con->execQuery("PRAGMA journal_mode = WAL;");
}

void SQLiteDatabase::Destroy(std::string source) {
	if (!con) return;

	if (source.empty()) LOG("Destroying SQLite connection!");
	else LOG("Destroying SQLite connection from %s!", source.c_str());

	con->close();
	delete con;
	con = nullptr;
}

void SQLiteDatabase::ExecuteCustomQuery(const std::string_view query) {
	con->compileStatement(query.data()).execDML();
}

CppSQLite3Statement SQLiteDatabase::CreatePreppedStmt(const std::string& query) {
	return con->compileStatement(query.c_str());
}

void SQLiteDatabase::Commit() {
	if (!con->IsAutoCommitOn()) con->compileStatement("COMMIT;").execDML();
}

bool SQLiteDatabase::GetAutoCommit() {
	return con->IsAutoCommitOn();
}

void SQLiteDatabase::SetAutoCommit(bool value) {
	if (value) {
		if (!GetAutoCommit()) con->compileStatement("COMMIT;").execDML();
	} else {
		if (GetAutoCommit()) con->compileStatement("BEGIN;").execDML();
	}
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
	ExecuteDelete("DELETE FROM charinfo WHERE id=?;", characterId);
}
