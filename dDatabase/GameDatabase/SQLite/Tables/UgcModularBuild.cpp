#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<uint32_t> characterId) {
	ExecuteInsert("INSERT INTO ugc_modular_build (ugc_id, ldf_config, character_id) VALUES (?,?,?)", bigId, modules, characterId);
}

void SQLiteDatabase::DeleteUgcBuild(const LWOOBJID bigId) {
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE ugc_id = ?;", bigId);
}
