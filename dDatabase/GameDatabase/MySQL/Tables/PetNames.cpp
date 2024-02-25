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
