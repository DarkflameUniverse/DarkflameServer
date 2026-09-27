#include "Database.h"
#include "Game.h"
#include "dConfig.h"
#include "Logger.h"
#include "DluAssert.h"

#include "SQLiteDatabase.h"
#include "MySQLDatabase.h"

#include <filesystem>
#include <ranges>
#include <stdexcept>

#include "BinaryPathFinder.h"

#pragma warning (disable:4251) //Disables SQL warnings

namespace {
	GameDatabase* database = nullptr;

	/**
	 * A replay sandbox (replay_sandbox=1, docs/CaptureReplay.md) only ever uses its own SQLite file, inside its own
	 * folder, and never the live one: anything else and the server refuses to start.
	 */
	void CheckSandbox(const std::string& databaseType) {
		if (!Game::config || Game::config->GetValue("replay_sandbox") != "1") return;
		if (databaseType != "sqlite") throw std::runtime_error("A replay sandbox only runs on its own SQLite database (database_type=sqlite)");
		namespace fs = std::filesystem;
		const auto folder = fs::weakly_canonical(BinaryPathFinder::GetBinaryDir());
		const auto path = fs::weakly_canonical(folder / Game::config->GetValue("sqlite_database_path"));
		const auto relative = path.lexically_relative(folder);
		if (Game::config->GetValue("sqlite_database_path").empty() || relative.empty() || *relative.begin() == "..") {
			throw std::runtime_error("A replay sandbox's database must be inside its own folder (" + folder.string() + "), not " + path.string());
		}
		const auto live = Game::config->GetValue("replay_live_sqlite_path");
		std::error_code ec;
		if (!live.empty() && fs::exists(live, ec) && fs::equivalent(path, live, ec)) {
			throw std::runtime_error("A replay sandbox must not use the live database " + live);
		}
	}
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
	CheckSandbox(databaseType);

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

std::unique_ptr<GameDatabase> Database::CreateConnection() {
	std::unique_ptr<GameDatabase> connection;
	CheckSandbox(GetMigrationFolder());
	if (GetMigrationFolder() == "sqlite") connection = std::make_unique<SQLiteDatabase>();
	else connection = std::make_unique<MySQLDatabase>();
	connection->Connect();
	return connection;
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
