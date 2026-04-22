#include "SQLiteDatabase.h"

void SQLiteDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) {
	ExecuteInsert(
		"INSERT INTO `pet_names` (`id`, `pet_name`, `approved`) VALUES (?, ?, ?) "
		"ON CONFLICT(id) DO UPDATE SET pet_name = ?, approved = ?;",
		petId,
		info.petName,
		info.approvalStatus,
		info.petName,
		info.approvalStatus);
}

std::optional<IPetNames::Info> SQLiteDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	auto [_, result] = ExecuteSelect("SELECT pet_name, approved FROM pet_names WHERE id = ? LIMIT 1;", petId);

	if (result.eof()) {
		return std::nullopt;
	}

	IPetNames::Info toReturn;
	toReturn.petName = result.getStringField("pet_name");
	toReturn.approvalStatus = result.getIntField("approved");

	return toReturn;
}

std::vector<IPetNames::DetailedInfo> SQLiteDatabase::GetAllPetNames() {
	std::vector<IPetNames::DetailedInfo> out;
	auto [stmt, res] = ExecuteSelect("SELECT id, pet_name, approved, owner_id FROM pet_names ORDER BY id DESC;");

	while (!res.eof()) {
		IPetNames::DetailedInfo info;
		info.id = static_cast<LWOOBJID>(res.getInt64Field("id"));
		info.petName = res.getStringField("pet_name");
		info.approvalStatus = res.getIntField("approved");
		info.ownerId = static_cast<LWOOBJID>(res.getInt64Field("owner_id"));
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::vector<IPetNames::DetailedInfo> SQLiteDatabase::GetPetNamesByStatus(int32_t status) {
	std::vector<IPetNames::DetailedInfo> out;
	auto [stmt, res] = ExecuteSelect("SELECT id, pet_name, approved, owner_id FROM pet_names WHERE approved = ? ORDER BY id DESC;", status);

	while (!res.eof()) {
		IPetNames::DetailedInfo info;
		info.id = static_cast<LWOOBJID>(res.getInt64Field("id"));
		info.petName = res.getStringField("pet_name");
		info.approvalStatus = res.getIntField("approved");
		info.ownerId = static_cast<LWOOBJID>(res.getInt64Field("owner_id"));
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

void SQLiteDatabase::SetPetApprovalStatus(const LWOOBJID& petId, int32_t status) {
	ExecuteUpdate("UPDATE pet_names SET approved = ? WHERE id = ?;", status, petId);
}

uint32_t SQLiteDatabase::GetPendingPetNamesCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM pet_names WHERE approved = 1;");
	return res.eof() ? 0 : res.getIntField("count");
}
