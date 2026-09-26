#include "MySQLDatabase.h"

std::optional<bool> MySQLDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto keyCheckRes = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (!keyCheckRes->next()) {
		return std::nullopt;
	}

	return keyCheckRes->getBoolean("active");
}

#include "json.hpp"

void MySQLDatabase::CreatePlayKey(const std::string_view keyString, const uint32_t uses, const std::string_view notes) {
	ExecuteInsert("INSERT INTO play_keys (key_string, key_uses, active, created_at, notes) VALUES (?, ?, 1, NOW(), ?);", keyString, uses, notes);
}

void MySQLDatabase::SetPlayKeyActive(const int32_t playkeyId, const bool active) {
	ExecuteUpdate("UPDATE play_keys SET active = ? WHERE id = ?;", active ? 1 : 0, playkeyId);
}

uint32_t MySQLDatabase::GetPlayKeyCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM play_keys;");
	return res->next() ? res->getUInt("count") : 0;
}

