#include "MigrationManager.h"

#include <fstream>
#include <string>

#include "dLogger.h"
#include "GeneralUtils.h"
#include "CDClientDatabase.h"
#include "BinaryPathFinder.h"

Migration Migration::LoadMigration(const std::string& type, const std::string& dbType, const std::string& name) {
	Migration migration{};
	std::ifstream file(BinaryPathFinder::GetBinaryDir() / "migrations/" / type / dbType / name);

	if (file.is_open()) {
		std::string line;
		std::string total = "";

		while (std::getline(file, line)) {
			total += line;
		}

		file.close();

		migration.name = ;
		migration.data = total;
	}

	return migration;
}

void MigrationManager::RunSQLiteMigrations() {
	auto cdstmt = CDClientDatabase::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);");
	cdstmt.execQuery().finalize();
	cdstmt.finalize();

	for (const auto& entry : GeneralUtils::GetSqlFileNamesFromFolder((BinaryPathFinder::GetBinaryDir() / "migrations/cdserver/").string())) {
		auto migration = Migration::LoadMigration("cdserver", "", entry);

		if (migration.data.empty()) continue;

		// Check if there is an entry in the migration history table on the cdclient database.
		cdstmt = CDClientDatabase::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		cdstmt.bind((int32_t)1, migration.name.c_str());
		auto cdres = cdstmt.execQuery();
		bool doExit = !cdres.eof();
		cdres.finalize();
		cdstmt.finalize();

		if (doExit) continue;

		// Check first if there is entry in the migration history table on the main database.
		stmt = Database::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		stmt->setString(1, migration.name.c_str());
		auto* res = stmt->executeQuery();
		doExit = res->next();
		delete res;
		delete stmt;
		if (doExit) {
			// Insert into cdclient database if there is an entry in the main database but not the cdclient database.
			cdstmt = CDClientDatabase::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
			cdstmt.bind((int32_t)1, migration.name.c_str());
			cdstmt.execQuery().finalize();
			cdstmt.finalize();
			continue;
		}

		// Doing these 1 migration at a time since one takes a long time and some may think it is crashing.
		// This will at the least guarentee that the full migration needs to be run in order to be counted as "migrated".
		Game::logger->Log("MigrationRunner", "Executing migration: %s.  This may take a while.  Do not shut down server.", migration.name.c_str());
		CDClientDatabase::ExecuteQuery("BEGIN TRANSACTION;");
		for (const auto& dml : GeneralUtils::SplitString(migration.data, ';')) {
			if (dml.empty()) continue;
			try {
				CDClientDatabase::ExecuteDML(dml.c_str());
			} catch (CppSQLite3Exception& e) {
				Game::logger->Log("MigrationRunner", "Encountered error running DML command: (%i) : %s", e.errorCode(), e.errorMessage());
			}
		}

		// Insert into cdclient database.
		cdstmt = CDClientDatabase::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		cdstmt.bind((int32_t)1, migration.name.c_str());
		cdstmt.execQuery().finalize();
		cdstmt.finalize();
		CDClientDatabase::ExecuteQuery("COMMIT;");
	}

	Game::logger->Log("MigrationRunner", "CDServer database is up to date.");
}
