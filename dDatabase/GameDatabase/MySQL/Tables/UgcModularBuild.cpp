#include "MySQLDatabase.h"

void MySQLDatabase::InsertUgcBuild(const std::string& modules, const LWOOBJID bigId, const std::optional<LWOOBJID> characterId) {
	ExecuteInsert("INSERT INTO ugc_modular_build (ugc_id, ldf_config, character_id) VALUES (?,?,?)", bigId, modules, characterId);
}

void MySQLDatabase::DeleteUgcBuild(const LWOOBJID bigId) {
	ExecuteDelete("DELETE FROM ugc_modular_build WHERE ugc_id = ?;", bigId);
}
