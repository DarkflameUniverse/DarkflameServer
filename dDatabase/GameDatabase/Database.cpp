#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "DluAssert.h"

#include "SQLiteDatabase.h"
#include "MySQLDatabase.h"

#include <ranges>

#pragma warning (disable:4251) //Disables SQL warnings

namespace {
	GameDatabase* database = nullptr;
}

std::string Database::GetMigrationFolder() {
	const std::set<std::string> validMysqlTypes = { "mysql", "mariadb", "maria" };
	auto databaseType = Game::config->GetValue("database_type");
	std::ranges::transform(databaseType, databaseType.begin(), ::tolower);
	if (databaseType == "sqlite") return "sqlite";
	else if (validMysqlTypes.contains(databaseType)) return "mysql";
	else {
		LOG("No database specified, using MySQL");
		return "mysql";
	}
}

void Database::Connect() {
	if (database) {
		LOG("Tried to connect to database when it's already connected!");
		return;
	}

	const auto databaseType = GetMigrationFolder();

	if (databaseType == "sqlite") database = new SQLiteDatabase();
	else if (databaseType == "mysql") database = new MySQLDatabase();
	else {
		LOG("Invalid database type specified in config, using MySQL");
		database = new MySQLDatabase();
	}

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
