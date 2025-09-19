#include "MySQLDatabase.h"

std::string MySQLDatabase::GetCharacterXml(const LWOOBJID charId) {
	auto result = ExecuteSelect("SELECT xml_data FROM charxml WHERE id = ? LIMIT 1;", charId);

	if (!result->next()) {
		return "";
	}

	return result->getString("xml_data").c_str();
}

void MySQLDatabase::UpdateCharacterXml(const LWOOBJID charId, const std::string_view lxfml) {
	ExecuteUpdate("UPDATE charxml SET xml_data = ? WHERE id = ?;", lxfml, charId);
}

void MySQLDatabase::InsertCharacterXml(const LWOOBJID characterId, const std::string_view lxfml) {
	ExecuteInsert("INSERT INTO `charxml` (`id`, `xml_data`) VALUES (?,?)", characterId, lxfml);
}
