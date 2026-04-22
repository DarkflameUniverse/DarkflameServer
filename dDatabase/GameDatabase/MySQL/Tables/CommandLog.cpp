#include "MySQLDatabase.h"

void MySQLDatabase::InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) {
	ExecuteInsert("INSERT INTO command_log (character_id, command) VALUES (?, ?);", characterId, command);
}

std::vector<ICommandLog::Entry> MySQLDatabase::GetCommandLogs(uint32_t limit) {
	std::vector<ICommandLog::Entry> logs;
	auto res = ExecuteSelect("SELECT id, character_id, command FROM command_log ORDER BY id DESC LIMIT ?;", limit);

	while (res->next()) {
		ICommandLog::Entry entry;
		entry.timestamp = 0;  // Timestamp column doesn't exist in command_log table
		entry.characterId = res->getInt64("character_id");
		entry.command = res->getString("command").c_str();
		entry.arguments = "";  // Arguments not currently stored in DB
		logs.push_back(entry);
	}

	return logs;
}
