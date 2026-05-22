#include "MySQLDatabase.h"

int64_t MySQLDatabase::GetCharacterReputation(const LWOOBJID charId) {
	auto result = ExecuteSelect("SELECT reputation FROM character_reputation WHERE character_id = ? LIMIT 1;", charId);

	if (!result->next()) {
		return 0;
	}

	return result->getInt64("reputation");
}

void MySQLDatabase::SetCharacterReputation(const LWOOBJID charId, const int64_t reputation) {
	ExecuteInsert(
		"INSERT INTO character_reputation (character_id, reputation) VALUES (?, ?) ON DUPLICATE KEY UPDATE reputation = ?;",
		charId, reputation, reputation);
}
