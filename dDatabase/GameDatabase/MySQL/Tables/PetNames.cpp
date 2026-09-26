#include "MySQLDatabase.h"
#include "GeneralUtils.h"
#include "json.hpp"

void MySQLDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) {
	const auto owner = info.ownerId == 0 ? std::optional<LWOOBJID>{} : std::optional<LWOOBJID>{ info.ownerId };
	ExecuteInsert(
		"INSERT INTO `pet_names` (`id`, `pet_name`, `approved`, `owner_id`) VALUES (?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE pet_name = ?, approved = ?, owner_id = COALESCE(?, owner_id);",
		petId,
		info.petName,
		info.approvalStatus,
		owner,
		info.petName,
		info.approvalStatus,
		owner);
}

std::optional<IPetNames::Info> MySQLDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	auto result = ExecuteSelect("SELECT pet_name, approved, owner_id FROM pet_names WHERE id = ? LIMIT 1;", petId);

	if (!result->next()) {
		return std::nullopt;
	}

	IPetNames::Info toReturn;
	toReturn.petName = result->getString("pet_name").c_str();
	toReturn.approvalStatus = result->getInt("approved");
	toReturn.ownerId = result->isNull("owner_id") ? 0 : result->getInt64("owner_id");

	return toReturn;
}

std::string MySQLDatabase::GetPetNamesTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, bool pendingOnly) {
	// A number in the search box also matches IDs exactly (-1 never matches); names match the pet or its owner
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	const std::string from = " FROM pet_names p LEFT JOIN charinfo c ON c.id = p.owner_id";
	std::string whereClause;
	// approved: 1 = pending moderation, 2 = approved (see PetComponent)
	const std::string pendingFilter = pendingOnly ? "p.approved = 1" : "";
	const std::string searchFilter = "(p.pet_name LIKE CONCAT('%', ?, '%') OR p.id = ? OR c.name LIKE CONCAT('%', ?, '%'))";
	if (pendingOnly && !search.empty()) whereClause = " WHERE " + pendingFilter + " AND " + searchFilter;
	else if (pendingOnly) whereClause = " WHERE " + pendingFilter;
	else if (!search.empty()) whereClause = " WHERE " + searchFilter;

	std::string orderColumnName = "p.id";
	switch (orderColumn) {
		case 0: orderColumnName = "p.id"; break;
		case 1: orderColumnName = "p.pet_name"; break;
		case 2: orderColumnName = "p.approved"; break;
		case 3: orderColumnName = "c.name"; break;
	}
	std::string orderClause = " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC");
	std::string mainQuery = "SELECT p.id, p.pet_name, p.approved, p.owner_id, c.name AS owner_name" + from + whereClause + orderClause + " LIMIT ?, ?;";

	auto totalCountResult = ExecuteSelect("SELECT COUNT(*) as count FROM pet_names p" + (pendingOnly ? " WHERE " + pendingFilter : std::string{}) + ";");
	uint32_t totalRecords = totalCountResult->next() ? totalCountResult->getUInt("count") : 0;

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto filteredCountResult = ExecuteSelect("SELECT COUNT(*) as count" + from + whereClause + ";", search, searchId, search);
		filteredRecords = filteredCountResult->next() ? filteredCountResult->getUInt("count") : 0;
	}

	auto result = !search.empty()
		? ExecuteSelect(mainQuery, search, searchId, search, start, length)
		: ExecuteSelect(mainQuery, start, length);

	nlohmann::json dataArray = nlohmann::json::array();
	while (result->next()) {
		const bool hasOwner = !result->isNull("owner_id") && result->getInt64("owner_id") != 0;
		dataArray.push_back({
			{"id", std::to_string(result->getInt64("id"))},
			{"pet_name", result->getString("pet_name")},
			{"approved", result->getInt("approved")},
			{"owner_id", hasOwner ? std::to_string(result->getInt64("owner_id")) : ""},
			{"owner_name", std::string(result->isNull("owner_name") ? "" : result->getString("owner_name").c_str())}
		});
	}
	return nlohmann::json({{"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", dataArray}}).dump();
}

void MySQLDatabase::ApprovePetName(const int64_t id) {
	ExecuteUpdate("UPDATE pet_names SET approved = 2 WHERE id = ?;", id);
}

void MySQLDatabase::RejectPetName(const int64_t id) {
	ExecuteDelete("DELETE FROM pet_names WHERE id = ?;", id);
}

std::vector<LWOOBJID> MySQLDatabase::GetPetsWithUnknownOwner() {
	std::vector<LWOOBJID> pets;
	auto result = ExecuteSelect("SELECT id FROM pet_names WHERE owner_id IS NULL;");
	while (result->next()) pets.push_back(result->getInt64("id"));
	return pets;
}

void MySQLDatabase::SetPetOwner(const LWOOBJID petId, const LWOOBJID ownerId) {
	ExecuteUpdate("UPDATE pet_names SET owner_id = ? WHERE id = ?;", ownerId, petId);
}
