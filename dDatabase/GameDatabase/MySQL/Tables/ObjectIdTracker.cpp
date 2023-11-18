#include "MySQLDatabase.h"

std::optional<uint32_t> MySQLDatabase::GetCurrentPersistentId() {
	auto result = ExecuteSelect("SELECT last_object_id FROM object_id_tracker");
	if (!result->next()) {
		return std::nullopt;
	}
	return result->getUInt("last_object_id");
}

void MySQLDatabase::InsertDefaultPersistentId() {
	ExecuteInsert("INSERT INTO object_id_tracker VALUES (1);");
}

void MySQLDatabase::UpdatePersistentId(const uint32_t newId) {
	ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = ?;", newId);
}
