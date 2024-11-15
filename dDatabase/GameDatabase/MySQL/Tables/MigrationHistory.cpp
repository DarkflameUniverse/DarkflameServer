#include "MySQLDatabase.h"

void MySQLDatabase::CreateMigrationHistoryTable() {
	ExecuteInsert("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP());");
}

bool MySQLDatabase::IsMigrationRun(const std::string_view str) {
	return ExecuteSelect("SELECT name FROM migration_history WHERE name = ?;", str)->next();
}

void MySQLDatabase::InsertMigration(const std::string_view str) {
	ExecuteInsert("INSERT INTO migration_history (name) VALUES (?);", str);
}
