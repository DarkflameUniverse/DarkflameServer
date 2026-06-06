#include "SQLiteDatabase.h"

std::string SQLiteDatabase::GetCharacterXml(const LWOOBJID charId) {
	auto [_, result] = ExecuteSelect("SELECT xml_data FROM charxml WHERE id = ? LIMIT 1;", charId);

	if (result.eof()) {
		return "";
	}

	return result.getStringField("xml_data");
}

void SQLiteDatabase::UpdateCharacterXml(const LWOOBJID charId, const std::string_view lxfml) {
	ExecuteUpdate("UPDATE charxml SET xml_data = ? WHERE id = ?;", lxfml, charId);
}

void SQLiteDatabase::InsertCharacterXml(const LWOOBJID characterId, const std::string_view lxfml) {
	ExecuteInsert("INSERT INTO `charxml` (`id`, `xml_data`) VALUES (?,?)", characterId, lxfml);
}

#include "json.hpp"

std::string SQLiteDatabase::GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT c.id, c.name, c.account_id, c.last_login, a.name as account_name FROM charinfo c JOIN accounts a ON c.account_id = a.id";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE c.name LIKE '%' || ? || '%'";
	}

	// Map column indices to database columns
	std::string orderColumnName = "c.id";
	switch (orderColumn) {
		case 0: orderColumnName = "c.id"; break;
		case 1: orderColumnName = "c.name"; break;
		case 2: orderColumnName = "a.name"; break;
		case 3: orderColumnName = "c.last_login"; break;
		default: orderColumnName = "c.id";
	}

	orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");

	// Build the main query
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM charinfo;";
	auto [__, totalCountResult] = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM charinfo WHERE name LIKE '%' || ? || '%';";
		auto [___, filteredCountResult] = ExecuteSelect(filteredCountQuery, search);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	// Execute main query
	auto [stmt, result] = !search.empty() ? 
		ExecuteSelect(mainQuery, search, length, start) :
		ExecuteSelect(mainQuery, length, start);

	// Build response JSON
	nlohmann::json charactersArray = nlohmann::json::array();

	while (!result.eof()) {
		nlohmann::json character = {
			{"id", result.getInt64Field("id")},
			{"name", result.getStringField("name")},
			{"account_name", result.getStringField("account_name")},
			{"last_login", result.getInt64Field("last_login")}
		};
		charactersArray.push_back(character);
		result.nextRow();
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", charactersArray}
	};

	return response.dump();
}
