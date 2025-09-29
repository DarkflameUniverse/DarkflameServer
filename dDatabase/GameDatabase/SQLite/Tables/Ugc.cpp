#include "SQLiteDatabase.h"

IUgc::Model ReadModel(CppSQLite3Query& result) {
	IUgc::Model model;

	int blobSize{};
	const auto* blob = result.getBlobField("lxfml", blobSize);
	model.lxfmlData << std::string(reinterpret_cast<const char*>(blob), blobSize);
	model.id = result.getInt64Field("ugcID");
	model.modelID = result.getInt64Field("modelID");

	return model;
}

std::vector<IUgc::Model> SQLiteDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	auto [_, result] = ExecuteSelect(
		"SELECT lxfml, u.id AS ugcID, pc.id AS modelID FROM ugc AS u JOIN properties_contents AS pc ON u.id = pc.ugc_id WHERE lot = 14 AND property_id = ? AND pc.ugc_id IS NOT NULL;",
		propertyId);

	std::vector<IUgc::Model> toReturn;

	while (!result.eof()) {
		toReturn.push_back(ReadModel(result));
		result.nextRow();
	}

	return toReturn;
}

std::vector<IUgc::Model> SQLiteDatabase::GetAllUgcModels() {
	auto [_, result] = ExecuteSelect("SELECT u.id AS ugcID, pc.id AS modelID, lxfml FROM ugc AS u JOIN properties_contents AS pc ON pc.id = u.id;");

	std::vector<IUgc::Model> models;
	while (!result.eof()) {
		models.push_back(ReadModel(result));
		result.nextRow();
	}

	return models;
}

void SQLiteDatabase::RemoveUnreferencedUgcModels() {
	ExecuteDelete("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents WHERE ugc_id IS NOT NULL);");
}

void SQLiteDatabase::InsertNewUgcModel(
	std::stringstream& sd0Data, // cant be const sad
	const uint64_t blueprintId,
	const uint32_t accountId,
	const LWOOBJID characterId) {
	const std::istream stream(sd0Data.rdbuf());
	ExecuteInsert(
		"INSERT INTO `ugc`(`id`, `account_id`, `character_id`, `is_optimized`, `lxfml`, `bake_ao`, `filename`) VALUES (?,?,?,?,?,?,?)",
		blueprintId,
		accountId,
		characterId,
		0,
		&stream,
		false,
		"weedeater.lxfml"
	);
}

void SQLiteDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {
	ExecuteDelete("DELETE FROM ugc WHERE id = ?;", modelId);
	ExecuteDelete("DELETE FROM properties_contents WHERE ugc_id = ?;", modelId);
}

void SQLiteDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) {
	const std::istream stream(lxfml.rdbuf());
	ExecuteUpdate("UPDATE ugc SET lxfml = ? WHERE id = ?;", &stream, modelId);
}

std::optional<IUgc::Model> SQLiteDatabase::GetUgcModel(const LWOOBJID ugcId) {
	auto [_, result] = ExecuteSelect("SELECT u.id AS ugcID, pc.id AS modelID, lxfml FROM ugc AS u JOIN properties_contents AS pc ON pc.id = u.id WHERE u.id = ?;", ugcId);

	std::optional<IUgc::Model> toReturn = std::nullopt;
	if (!result.eof()) {
		toReturn = ReadModel(result);
	}

	return toReturn;
}
