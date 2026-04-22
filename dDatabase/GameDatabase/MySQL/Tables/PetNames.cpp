#include "MySQLDatabase.h"

void MySQLDatabase::SetPetNameModerationStatus(const LWOOBJID& petId, const IPetNames::Info& info) {
	ExecuteInsert(
		"INSERT INTO `pet_names` (`id`, `pet_name`, `approved`) VALUES (?, ?, ?) "
		"ON DUPLICATE KEY UPDATE pet_name = ?, approved = ?;",
		petId,
		info.petName,
		info.approvalStatus,
		info.petName,
		info.approvalStatus);
}

std::optional<IPetNames::Info> MySQLDatabase::GetPetNameInfo(const LWOOBJID& petId) {
	auto result = ExecuteSelect("SELECT pet_name, approved FROM pet_names WHERE id = ? LIMIT 1;", petId);

	if (!result->next()) {
		return std::nullopt;
	}

	IPetNames::Info toReturn;
	toReturn.petName = result->getString("pet_name").c_str();
	toReturn.approvalStatus = result->getInt("approved");

	return toReturn;
}

std::vector<IPetNames::DetailedInfo> MySQLDatabase::GetAllPetNames() {
	std::vector<IPetNames::DetailedInfo> out;
	auto res = ExecuteSelect("SELECT id, pet_name, approved, owner_id FROM pet_names ORDER BY id DESC;");

	while (res->next()) {
		IPetNames::DetailedInfo info;
		info.id = res->getUInt64("id");
		info.petName = res->getString("pet_name").c_str();
		info.approvalStatus = res->getInt("approved");
		info.ownerId = res->getUInt64("owner_id");
		out.push_back(info);
	}

	return out;
}

std::vector<IPetNames::DetailedInfo> MySQLDatabase::GetPetNamesByStatus(int32_t status) {
	std::vector<IPetNames::DetailedInfo> out;
	auto res = ExecuteSelect("SELECT id, pet_name, approved, owner_id FROM pet_names WHERE approved = ? ORDER BY id DESC;", status);

	while (res->next()) {
		IPetNames::DetailedInfo info;
		info.id = res->getUInt64("id");
		info.petName = res->getString("pet_name").c_str();
		info.approvalStatus = res->getInt("approved");
		info.ownerId = res->getUInt64("owner_id");
		out.push_back(info);
	}

	return out;
}

void MySQLDatabase::SetPetApprovalStatus(const LWOOBJID& petId, int32_t status) {
	ExecuteUpdate("UPDATE pet_names SET approved = ? WHERE id = ?;", status, petId);
}

uint32_t MySQLDatabase::GetPendingPetNamesCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM pet_names WHERE approved = 1;");
	return res->next() ? res->getUInt("count") : 0;
}
