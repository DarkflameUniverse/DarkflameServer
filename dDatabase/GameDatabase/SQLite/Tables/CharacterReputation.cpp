#include "SQLiteDatabase.h"

int64_t SQLiteDatabase::GetCharacterReputation(const LWOOBJID charId) {
	auto [_, result] = ExecuteSelect("SELECT reputation FROM character_reputation WHERE character_id = ? LIMIT 1;", charId);

	if (result.eof()) {
		return 0;
	}

	return result.getInt64Field("reputation");
}

void SQLiteDatabase::SetCharacterReputation(const LWOOBJID charId, const int64_t reputation) {
	ExecuteInsert(
		"INSERT OR REPLACE INTO character_reputation (character_id, reputation) VALUES (?, ?);",
		charId, reputation);
}
