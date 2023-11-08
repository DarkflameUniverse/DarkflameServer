#include "MySQLDatabase.h"

std::optional<bool> MySQLDatabase::IsPlaykeyActive(const uint32_t playkeyId) {
	auto keyCheckRes = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (!keyCheckRes->next()) {
		return std::nullopt;
	}

	return keyCheckRes->getBoolean("active");
}
