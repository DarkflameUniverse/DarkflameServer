#include "MigrationRunner.h"

#include "BrickByBrickFix.h"
#include "GeneralUtils.h"

#include <fstream>
#include <algorithm>
#include <thread>

void MigrationRunner::RunMigrations() {
	auto* stmt = Database::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
	stmt->execute();
	delete stmt;

	sql::SQLString finalSQL = "";
	Migration checkMigration{};
	bool runSd0Migrations = false;
	for (const auto& entry : GeneralUtils::GetFileNamesFromFolder("./migrations/")) {
		auto migration = LoadMigration(entry);

		if (migration.data.empty()) {
			continue;
		}

		checkMigration = migration;

		stmt = Database::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
		stmt->setString(1, migration.name);
		auto* res = stmt->executeQuery();
		bool doExit = res->next();
		delete res;
		delete stmt;
		if (doExit) continue;

		Game::logger->Log("MigrationRunner", "Running migration: %s", migration.name.c_str());
		if (migration.name == "5_brick_model_sd0.sql") {
			runSd0Migrations = true;
		} else {
			finalSQL.append(migration.data);
		}

		stmt = Database::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
		stmt->setString(1, entry);
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
				simpleStatement->execute(query);
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

Migration MigrationRunner::LoadMigration(std::string path) {
	Migration migration{};
	std::ifstream file("./migrations/" + path);

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
