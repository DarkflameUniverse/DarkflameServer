#include "MigrationRunner.h"

#include "GeneralUtils.h"

#include <fstream>
#include <algorithm>
#include <thread>

void MigrationRunner::RunMigrations() {
	auto stmt = Database::CreatePreppedStmt("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
	stmt->executeQuery();
    delete stmt;

    sql::SQLString finalSQL = "";
    Migration checkMigration{};

    for (const auto& entry : GeneralUtils::GetFileNamesFromFolder("./migrations/")) {
        auto migration = LoadMigration(entry);

        if (migration.data.empty()) {
            continue;
        }

        checkMigration = migration;

        stmt = Database::CreatePreppedStmt("SELECT name FROM migration_history WHERE name = ?;");
        stmt->setString(1, migration.name);
        auto res = stmt->executeQuery();
        bool doExit = res->next();
        delete res;
        delete stmt;
        if (doExit) continue;

        Game::logger->Log("MigrationRunner", "Running migration: " + migration.name + "\n");

        finalSQL.append(migration.data);
        finalSQL.append('\n');

        stmt = Database::CreatePreppedStmt("INSERT INTO migration_history (name) VALUES (?);");
        stmt->setString(1, entry);
        stmt->execute();
        delete stmt;
    }

    if (!finalSQL.empty()) {
        try {
            auto simpleStatement = Database::CreateStmt();
            simpleStatement->execute(finalSQL);
            delete simpleStatement;
        }
        catch (sql::SQLException e) {
            Game::logger->Log("MigrationRunner", std::string("Encountered error running migration: ") + e.what() + "\n");
        }
    }
}

Migration MigrationRunner::LoadMigration(std::string path) {
    Migration migration{};
    std::ifstream file("./migrations/" + path);

    if (file.is_open()) {
        std::hash<std::string> hash;
        
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
