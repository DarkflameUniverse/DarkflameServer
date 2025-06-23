#include "MigrationRunner.h"

#include "BrickByBrickFix.h"
#include "CDClientDatabase.h"
#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "BinaryPathFinder.h"
#include "ModelNormalizeMigration.h"

#include <fstream>

Migration LoadMigration(std::string folder, std::string path) {
	Migration migration{};
	std::ifstream file(BinaryPathFinder::GetBinaryDir() / "migrations/" / folder / path);

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

	// has to be here because when moving the files to the new folder, the migration_history table is not updated so it will run them all again.

	const auto migrationFolder = Database::GetMigrationFolder();
	if (!Database::Get()->IsMigrationRun("17_migration_for_migrations.sql") && migrationFolder == "mysql") {
		LOG("Running migration: 17_migration_for_migrations.sql");
		Database::Get()->ExecuteCustomQuery("UPDATE `migration_history` SET `name` = SUBSTR(`name`, 5) WHERE `name` LIKE \"dlu%\";");
		Database::Get()->InsertMigration("17_migration_for_migrations.sql");
	}

	std::string finalSQL = "";
	bool runSd0Migrations = false;
	bool runNormalizeMigrations = false;
	bool runNormalizeAfterFirstPartMigrations = false;
	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "./migrations/dlu/" / migrationFolder).string())) {
		auto migration = LoadMigration("dlu/" + migrationFolder + "/", entry);

		if (migration.data.empty()) {
			continue;
		}

		if (Database::Get()->IsMigrationRun(migration.name)) continue;

		LOG("Running migration: %s", migration.name.c_str());
		if (migration.name == "5_brick_model_sd0.sql") {
			runSd0Migrations = true;
		} else if (migration.name.ends_with("_normalize_model_positions.sql")) {
			runNormalizeMigrations = true;
		} else if (migration.name.ends_with("_normalize_model_positions_after_first_part.sql")) {
			runNormalizeAfterFirstPartMigrations = true;
		} else {
			finalSQL.append(migration.data.c_str());
		}

		Database::Get()->InsertMigration(migration.name);
	}

	if (finalSQL.empty() && !runSd0Migrations && !runNormalizeMigrations && !runNormalizeAfterFirstPartMigrations) {
		LOG("Server database is up to date.");
		return;
	}

	if (!finalSQL.empty()) {
		auto migration = GeneralUtils::SplitString(finalSQL, ';');
		for (auto& query : migration) {
			try {
				if (query.empty()) continue;
				Database::Get()->ExecuteCustomQuery(query);
			} catch (std::exception& e) {
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

	if (runNormalizeMigrations) {
		ModelNormalizeMigration::Run();
	}

	if (runNormalizeAfterFirstPartMigrations) {
		ModelNormalizeMigration::RunAfterFirstPart();
	}
}

void MigrationRunner::RunSQLiteMigrations() {
	auto cdstmt = CDClientDatabase::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);");
	cdstmt.execQuery().finalize();
	cdstmt.finalize();

	if (CDClientDatabase::ExecuteQuery("select * from migration_history where name = \"7_migration_for_migrations.sql\";").eof()) {
		LOG("Running migration: 7_migration_for_migrations.sql");
		CDClientDatabase::ExecuteQuery("UPDATE `migration_history` SET `name` = SUBSTR(`name`, 10) WHERE `name` LIKE \"cdserver%\";");
		CDClientDatabase::ExecuteQuery("INSERT INTO migration_history (name) VALUES (\"7_migration_for_migrations.sql\");");
	}

	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "migrations/cdserver/").string())) {
		auto migration = LoadMigration("cdserver/", entry);

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
