#include "SQLiteDatabase.h"

std::optional<uint32_t> SQLiteDatabase::GetCurrentPersistentId() {
	auto [_, result] = ExecuteSelect("SELECT last_object_id FROM object_id_tracker");
	if (result.eof()) {
		return std::nullopt;
	}
	return result.getIntField("last_object_id");
}

void SQLiteDatabase::InsertDefaultPersistentId() {
	ExecuteInsert("INSERT INTO object_id_tracker VALUES (1);");
}

void SQLiteDatabase::UpdatePersistentId(const uint32_t newId) {
	ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = ?;", newId);
}
