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
