#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) {
	ExecuteInsert("INSERT INTO command_log (character_id, command) VALUES (?, ?);", characterId, command);
}

std::vector<ICommandLog::Entry> SQLiteDatabase::GetCommandLogs(uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT id, character_id, command FROM command_log ORDER BY id DESC LIMIT ?;", limit);

	std::vector<ICommandLog::Entry> logs;
	while (!result.eof()) {
		ICommandLog::Entry entry;
		entry.timestamp = 0;  // Timestamp column doesn't exist in command_log table
		entry.characterId = result.getInt64Field("character_id");
		entry.command = result.getStringField("command");
		entry.arguments = "";  // Arguments not currently stored in DB
		logs.push_back(entry);
		result.nextRow();
	}

	return logs;
}
