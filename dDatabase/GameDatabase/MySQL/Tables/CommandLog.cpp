#include "MySQLDatabase.h"

void MySQLDatabase::InsertSlashCommandUsage(const LWOOBJID characterId, const std::string_view command) {
	ExecuteInsert("INSERT INTO command_log (character_id, command) VALUES (?, ?);", characterId, command);
}
