#include "SQLiteDatabase.h"

std::string SQLiteDatabase::GetCharacterXml(const uint32_t charId) {
	auto [_, result] = ExecuteSelect("SELECT xml_data FROM charxml WHERE id = ? LIMIT 1;", charId);

	if (result.eof()) {
		return "";
	}

	return result.getStringField("xml_data");
}

void SQLiteDatabase::UpdateCharacterXml(const uint32_t charId, const std::string_view lxfml) {
	ExecuteUpdate("UPDATE charxml SET xml_data = ? WHERE id = ?;", lxfml, charId);
}

void SQLiteDatabase::InsertCharacterXml(const uint32_t characterId, const std::string_view lxfml) {
	ExecuteInsert("INSERT INTO `charxml` (`id`, `xml_data`) VALUES (?,?)", characterId, lxfml);
}
