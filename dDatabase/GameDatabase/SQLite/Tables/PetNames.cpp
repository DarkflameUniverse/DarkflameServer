#include "SQLiteDatabase.h"
#include "GeneralUtils.h"
#include "json.hpp"

void SQLiteDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) {
	const auto owner = info.ownerId == 0 ? std::optional<LWOOBJID>{} : std::optional<LWOOBJID>{ info.ownerId };
	const auto lot = info.petLot <= 0 ? std::optional<uint32_t>{} : std::optional<uint32_t>{ static_cast<uint32_t>(info.petLot) };
	ExecuteInsert(
		"INSERT INTO `pet_names` (`id`, `pet_name`, `approved`, `owner_id`, `pet_lot`) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(id) DO UPDATE SET pet_name = ?, approved = ?, owner_id = COALESCE(?, owner_id), pet_lot = COALESCE(?, pet_lot);",
		petId,
		info.petName,
		info.approvalStatus,
		owner,
		lot,
		info.petName,
		info.approvalStatus,
		owner,
		lot);
}

std::optional<IPetNames::Info> SQLiteDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	auto [_, result] = ExecuteSelect("SELECT pet_name, approved, owner_id, pet_lot FROM pet_names WHERE id = ? LIMIT 1;", petId);

	if (result.eof()) {
		return std::nullopt;
	}

	IPetNames::Info toReturn;
	toReturn.petName = result.getStringField("pet_name");
	toReturn.approvalStatus = result.getIntField("approved");
	toReturn.ownerId = result.fieldIsNull("owner_id") ? 0 : result.getInt64Field("owner_id");
	toReturn.petLot = result.fieldIsNull("pet_lot") ? 0 : result.getIntField("pet_lot");

	return toReturn;
}

std::string SQLiteDatabase::GetPetNamesTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, bool pendingOnly) {
	// A number in the search box also matches IDs exactly (-1 never matches); names match the pet or its owner
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	const std::string from = " FROM pet_names p LEFT JOIN charinfo c ON c.id = p.owner_id";
	std::string whereClause;
	// approved: 1 = pending moderation, 2 = approved (see PetComponent)
	const std::string pendingFilter = pendingOnly ? "p.approved = 1" : "";
	const std::string searchFilter = "(p.pet_name LIKE '%' || ? || '%' OR p.id = ? OR c.name LIKE '%' || ? || '%')";
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
	std::string mainQuery = "SELECT p.id, p.pet_name, p.approved, p.owner_id, p.pet_lot, c.name AS owner_name" + from + whereClause + orderClause + " LIMIT ? OFFSET ?;";

	auto [__, totalCountResult] = ExecuteSelect("SELECT COUNT(*) as count FROM pet_names p" + (pendingOnly ? " WHERE " + pendingFilter : std::string{}) + ";");
	uint32_t totalRecords = totalCountResult.eof() ? 0 : totalCountResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [___, filteredCountResult] = ExecuteSelect("SELECT COUNT(*) as count" + from + whereClause + ";", search, searchId, search);
		filteredRecords = filteredCountResult.eof() ? 0 : filteredCountResult.getIntField("count");
	}

	auto [stmt, result] = !search.empty()
		? ExecuteSelect(mainQuery, search, searchId, search, length, start)
		: ExecuteSelect(mainQuery, length, start);

	nlohmann::json dataArray = nlohmann::json::array();
	while (!result.eof()) {
		const bool hasOwner = !result.fieldIsNull("owner_id") && result.getInt64Field("owner_id") != 0;
		dataArray.push_back({
			{"id", std::to_string(result.getInt64Field("id"))},
			{"pet_name", result.getStringField("pet_name")},
			{"approved", result.getIntField("approved")},
			{"owner_id", hasOwner ? std::to_string(result.getInt64Field("owner_id")) : ""},
			{"lot", result.fieldIsNull("pet_lot") ? 0 : result.getIntField("pet_lot")},
			{"owner_name", std::string(result.fieldIsNull("owner_name") ? "" : result.getStringField("owner_name"))}
		});
		result.nextRow();
	}
	return nlohmann::json({{"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", dataArray}}).dump();
}

void SQLiteDatabase::ApprovePetName(const int64_t id) {
	ExecuteUpdate("UPDATE pet_names SET approved = 2 WHERE id = ?;", id);
}

void SQLiteDatabase::RejectPetName(const int64_t id) {
	ExecuteDelete("DELETE FROM pet_names WHERE id = ?;", id);
}

std::vector<LWOOBJID> SQLiteDatabase::GetPetsWithUnknownOwner() {
	std::vector<LWOOBJID> pets;
	auto [_, result] = ExecuteSelect("SELECT id FROM pet_names WHERE owner_id IS NULL;");
	for (; !result.eof(); result.nextRow()) pets.push_back(result.getInt64Field("id"));
	return pets;
}

void SQLiteDatabase::SetPetOwner(const LWOOBJID petId, const LWOOBJID ownerId) {
	ExecuteUpdate("UPDATE pet_names SET owner_id = ? WHERE id = ?;", ownerId, petId);
}

void SQLiteDatabase::SetPetLotIfMissing(const LWOOBJID petId, const LOT petLot) {
	if (petLot <= 0) return;
	ExecuteUpdate("UPDATE pet_names SET pet_lot = ? WHERE id = ? AND (pet_lot IS NULL OR pet_lot = 0);", petLot, petId);
}
