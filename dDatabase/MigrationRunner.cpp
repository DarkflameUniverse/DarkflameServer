#include "MigrationRunner.h"

#include "BrickByBrickFix.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "dLogger.h"
#include "BinaryPathFinder.h"

#include <istream>

Migration LoadMigration(std::string path) {
	Migration migration{};
	std::ifstream file(BinaryPathFinder::GetBinaryDir() / "migrations/" / path);

	if (file.is_open()) {
		std::string line;
		std::string total = "";

		while (std::getline(file, line)) {
			total += line;
		}

		file.close();

		migration.name = path;
		migration.data = total;
	}

	return migration;
}

void MigrationRunner::RunMigrations() {
	auto* stmt = Database::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
	stmt->execute();
	delete stmt;

	sql::SQLString finalSQL = "";
	bool runSd0Migrations = false;
	for (const auto& entry : GeneralUtils::GetFileNamesFromFolder(BinaryPathFinder::GetBinaryDir() / "./migrations/dlu/")) {
		auto migration = LoadMigration("dlu/" + entry);

		if (migration.data.empty()) {
			continue;
		}

		stmt = Database::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		stmt->setString(1, migration.name.c_str());
		auto* res = stmt->executeQuery();
		bool doExit = res->next();
		delete res;
		delete stmt;
		if (doExit) continue;

		Game::logger->Log("MigrationRunner", "Running migration: %s", migration.name.c_str());
		if (migration.name == "5_brick_model_sd0.sql") {
			runSd0Migrations = true;
		} else {
			finalSQL.append(migration.data.c_str());
		}

		stmt = Database::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		stmt->setString(1, migration.name.c_str());
		stmt->execute();
		delete stmt;
	}

	if (finalSQL.empty() && !runSd0Migrations) {
		Game::logger->Log("MigrationRunner", "Server database is up to date.");
		return;
	}

	if (!finalSQL.empty()) {
		auto migration = GeneralUtils::SplitString(static_cast<std::string>(finalSQL), ';');
		std::unique_ptr<sql::Statement> simpleStatement(Database::CreateStmt());
		for (auto& query : migration) {
			try {
				if (query.empty()) continue;
				simpleStatement->execute(query.c_str());
			} catch (sql::SQLException& e) {
				Game::logger->Log("MigrationRunner", "Encountered error running migration: %s", e.what());
			}
		}
	}

	// Do this last on the off chance none of the other migrations have been run yet.
	if (runSd0Migrations) {
		uint32_t numberOfUpdatedModels = BrickByBrickFix::UpdateBrickByBrickModelsToSd0();
		Game::logger->Log("MasterServer", "%i models were updated from zlib to sd0.", numberOfUpdatedModels);
		uint32_t numberOfTruncatedModels = BrickByBrickFix::TruncateBrokenBrickByBrickXml();
		Game::logger->Log("MasterServer", "%i models were truncated from the database.", numberOfTruncatedModels);
	}
}

void MigrationRunner::RunSQLiteMigrations() {
	auto* stmt = Database::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
	stmt->execute();
	delete stmt;

	for (const auto& entry : GeneralUtils::GetFileNamesFromFolder(BinaryPathFinder::GetBinaryDir() / "migrations/cdserver/")) {
		auto migration = LoadMigration("cdserver/" + entry);

		if (migration.data.empty()) continue;

		stmt = Database::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		stmt->setString(1, migration.name.c_str());
		auto* res = stmt->executeQuery();
		bool doExit = res->next();
		delete res;
		delete stmt;
		if (doExit) continue;

		// Doing these 1 migration at a time since one takes a long time and some may think it is crashing.
		// This will at the least guarentee that the full migration needs to be run in order to be counted as "migrated".
		Game::logger->Log("MigrationRunner", "Executing migration: %s.  This may take a while.  Do not shut down server.", migration.name.c_str());
		for (const auto& dml : GeneralUtils::SplitString(migration.data, ';')) {
			if (dml.empty()) continue;
			try {
				CDClientDatabase::ExecuteDML(dml.c_str());
			} catch (CppSQLite3Exception& e) {
				Game::logger->Log("MigrationRunner", "Encountered error running DML command: (%i) : %s", e.errorCode(), e.errorMessage());
			}
		}
		stmt = Database::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		stmt->setString(1, migration.name);
		stmt->execute();
		delete stmt;
	}
	Game::logger->Log("MigrationRunner", "CDServer database is up to date.");
}
