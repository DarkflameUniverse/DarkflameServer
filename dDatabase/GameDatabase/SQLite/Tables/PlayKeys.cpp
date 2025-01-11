#include "SQLiteDatabase.h"

std::optional<bool> SQLiteDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto [_, keyCheckRes] = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (keyCheckRes.eof()) {
		return std::nullopt;
	}

	return keyCheckRes.getIntField("active");
}
