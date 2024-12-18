#include "SQLiteDatabase.h"

void SQLiteDatabase::CreateMigrationHistoryTable() {
	ExecuteInsert("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP);");
}

bool SQLiteDatabase::IsMigrationRun(const std::string_view str) {
	return !ExecuteSelect("SELECT name FROM migration_history WHERE name = ?;", str).second.eof();
}

void SQLiteDatabase::InsertMigration(const std::string_view str) {
	ExecuteInsert("INSERT INTO migration_history (name) VALUES (?);", str);
}
