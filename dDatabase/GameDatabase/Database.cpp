#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "DluAssert.h"

#include "SQLiteDatabase.h"
#include "MySQLDatabase.h"

#pragma warning (disable:4251) //Disables SQL warnings

namespace {
	GameDatabase* database = nullptr;
}

void Database::Connect() {
	if (database) {
		LOG("Tried to connect to database when it's already connected!");
		return;
	}

	if (Game::config->GetValue("sqlite_database") == "1") database = new SQLiteDatabase();
	else if (Game::config->GetValue("mysql_database") == "1") database = new MySQLDatabase();

	database->Connect();
}

GameDatabase* Database::Get() {
	if (!database) {
		LOG("Tried to get database when it's not connected!");
		Connect();
	}
	return database;
}

void Database::Destroy(std::string source) {
	if (database) {
		database->Destroy(source);
		delete database;
		database = nullptr;
	} else {
		LOG("Trying to destroy database when it's not connected!");
	}
}

void Database::_setDatabase(GameDatabase* const db) {
	if (database) delete database;
	database = db;
}
