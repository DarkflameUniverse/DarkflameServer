#include "MigrationRunner.h"

#include "BrickByBrickFix.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "BinaryPathFinder.h"

#include <fstream>

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
	Database::Get()->CreateMigrationHistoryTable();

	sql::SQLString finalSQL = "";
	bool runSd0Migrations = false;
	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "./migrations/dlu/").string())) {
		auto migration = LoadMigration("dlu/" + entry);

		if (migration.data.empty()) {
			continue;
		}

		if (Database::Get()->IsMigrationRun(migration.name)) continue;

		LOG("Running migration: %s", migration.name.c_str());
		if (migration.name == "dlu/5_brick_model_sd0.sql") {
			runSd0Migrations = true;
		} else {
			finalSQL.append(migration.data.c_str());
		}

		Database::Get()->InsertMigration(migration.name);
	}

	if (finalSQL.empty() && !runSd0Migrations) {
		LOG("Server database is up to date.");
		return;
	}

	if (!finalSQL.empty()) {
		auto migration = GeneralUtils::SplitString(static_cast<std::string>(finalSQL), ';');
		for (auto& query : migration) {
			try {
				if (query.empty()) continue;
				Database::Get()->ExecuteCustomQuery(query.c_str());
			} catch (sql::SQLException& e) {
				LOG("Encountered error running migration: %s", e.what());
			}
		}
	}

	// Do this last on the off chance none of the other migrations have been run yet.
	if (runSd0Migrations) {
		uint32_t numberOfUpdatedModels = BrickByBrickFix::UpdateBrickByBrickModelsToSd0();
		LOG("%i models were updated from zlib to sd0.", numberOfUpdatedModels);
		uint32_t numberOfTruncatedModels = BrickByBrickFix::TruncateBrokenBrickByBrickXml();
		LOG("%i models were truncated from the database.", numberOfTruncatedModels);
	}
}

void MigrationRunner::RunSQLiteMigrations() {
	auto cdstmt = CDClientDatabase::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);");
	cdstmt.execQuery().finalize();
	cdstmt.finalize();

	Database::Get()->CreateMigrationHistoryTable();

	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "migrations/cdserver/").string())) {
		auto migration = LoadMigration("cdserver/" + entry);

		if (migration.data.empty()) continue;

		// Check if there is an entry in the migration history table on the cdclient database.
		cdstmt = CDClientDatabase::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		cdstmt.bind(static_cast<int32_t>(1), migration.name.c_str());
		auto cdres = cdstmt.execQuery();

		if (!cdres.eof()) continue;

		// Check first if there is entry in the migration history table on the main database.
		if (Database::Get()->IsMigrationRun(migration.name)) {
			// Insert into cdclient database if there is an entry in the main database but not the cdclient database.
			cdstmt = CDClientDatabase::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
			cdstmt.bind(static_cast<int32_t>(1), migration.name.c_str());
			cdstmt.execQuery();
			continue;
		}

		// Doing these 1 migration at a time since one takes a long time and some may think it is crashing.
		// This will at the least guarentee that the full migration needs to be run in order to be counted as "migrated".
		LOG("Executing migration: %s.  This may take a while.  Do not shut down server.", migration.name.c_str());
		CDClientDatabase::ExecuteQuery("BEGIN TRANSACTION;");
		for (const auto& dml : GeneralUtils::SplitString(migration.data, ';')) {
			if (dml.empty()) continue;
			try {
				CDClientDatabase::ExecuteDML(dml.c_str());
			} catch (CppSQLite3Exception& e) {
				LOG("Encountered error running DML command: (%i) : %s", e.errorCode(), e.errorMessage());
			}
		}

		// Insert into cdclient database.
		cdstmt = CDClientDatabase::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		cdstmt.bind(static_cast<int32_t>(1), migration.name.c_str());
		cdstmt.execQuery();
		CDClientDatabase::ExecuteQuery("COMMIT;");
	}

	LOG("CDServer database is up to date.");
}
