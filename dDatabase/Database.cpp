#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "MySQLDatabase.h"
#include "DluAssert.h"

#pragma warning (disable:4251) //Disables SQL warnings

namespace {
	GameDatabase* database = nullptr;
}

void Database::Connect() {
	if (database) {
		LOG("Tried to connect to database when it's already connected!");
		return;
	}

	database = new MySQLDatabase();
	database->Connect();
}

GameDatabase* Database::Get() {
	if (!database) {
		LOG("Tried to get database when it's not connected!");
		Connect();
	}
	return database;
}

void Database::Destroy(std::string source, bool log) {
	if (database) {
		database->Destroy(source, log);
		delete database;
		database = nullptr;
	} else {
		LOG("Trying to destroy database when it's not connected!");
	}
}
