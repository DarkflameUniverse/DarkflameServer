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

#include "json.hpp"

std::string MySQLDatabase::GetCharactersTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// Build base query
	std::string baseQuery = "SELECT c.id, c.name, c.account_id, c.last_login, a.name as account_name FROM charinfo c JOIN accounts a ON c.account_id = a.id";
	std::string whereClause;
	std::string orderClause;

	// Add search filter if provided
	if (!search.empty()) {
		whereClause = " WHERE c.name LIKE CONCAT('%', ?, '%')";
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
	std::string mainQuery = baseQuery + whereClause + orderClause + " LIMIT ?, ?;";

	// Get total count
	std::string totalCountQuery = "SELECT COUNT(*) as count FROM charinfo;";
	auto totalCountResult = ExecuteSelect(totalCountQuery);
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	// Get filtered count
	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		std::string filteredCountQuery = "SELECT COUNT(*) as count FROM charinfo WHERE name LIKE CONCAT('%', ?, '%');";
		auto filteredCountResult = ExecuteSelect(filteredCountQuery, search);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
	}

	// Execute main query
	std::unique_ptr<sql::ResultSet> result;
	if (!search.empty()) {
		result = ExecuteSelect(mainQuery, search, start, length);
	} else {
		result = ExecuteSelect(mainQuery, start, length);
	}

	// Build response JSON
	nlohmann::json charactersArray = nlohmann::json::array();

	while (result->next()) {
		nlohmann::json character = {
			{"id", result->getUInt64("id")},
			{"name", result->getString("name")},
			{"account_name", result->getString("account_name")},
			{"last_login", result->getUInt64("last_login")}
		};
		charactersArray.push_back(character);
	}

	nlohmann::json response = {
		{"draw", 0},
		{"recordsTotal", totalRecords},
		{"recordsFiltered", filteredRecords},
		{"data", charactersArray}
	};

	return response.dump();
}
