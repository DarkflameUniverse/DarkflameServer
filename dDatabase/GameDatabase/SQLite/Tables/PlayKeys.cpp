#include "SQLiteDatabase.h"

std::optional<bool> SQLiteDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto [_, keyCheckRes] = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (keyCheckRes.eof()) {
		return std::nullopt;
	}

	return keyCheckRes.getIntField("active");
}

#include "json.hpp"

void SQLiteDatabase::CreatePlayKey(const std::string_view keyString, const uint32_t uses, const std::string_view notes) {
	ExecuteInsert("INSERT INTO play_keys (key_string, key_uses, active, created_at, notes) VALUES (?, ?, 1, datetime('now'), ?);", keyString, uses, notes);
}

void SQLiteDatabase::SetPlayKeyActive(const int32_t playkeyId, const bool active) {
	ExecuteUpdate("UPDATE play_keys SET active = ? WHERE id = ?;", active ? 1 : 0, playkeyId);
}

uint32_t SQLiteDatabase::GetPlayKeyCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM play_keys;");
	if (res.eof()) return 0;
	return res.getIntField("count");
}

