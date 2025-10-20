#include "MySQLDatabase.h"

IUgc::Model ReadModel(UniqueResultSet& result) {
	IUgc::Model model;

	// blob is owned by the query, so we need to do a deep copy :/
	std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
	model.lxfmlData << blob->rdbuf();
	model.id = result->getUInt64("ugcID");
	model.modelID = result->getUInt64("modelID");

	return model;
}

std::vector<IUgc::Model> MySQLDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	auto result = ExecuteSelect(
		"SELECT lxfml, u.id as ugcID, pc.id as modelID FROM ugc AS u JOIN properties_contents AS pc ON u.id = pc.ugc_id WHERE lot = 14 AND property_id = ? AND pc.ugc_id IS NOT NULL;",
		propertyId);

	std::vector<IUgc::Model> toReturn;

	while (result->next()) {
		toReturn.push_back(ReadModel(result));
	}

	return toReturn;
}

std::vector<IUgc::Model> MySQLDatabase::GetAllUgcModels() {
	auto result = ExecuteSelect("SELECT u.id AS ugcID, lxfml, pc.id AS modelID FROM ugc AS u JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE pc.lot = 14 AND pc.ugc_id IS NOT NULL;");

	std::vector<IUgc::Model> models;
	models.reserve(result->rowsCount());
	while (result->next()) {
		models.push_back(ReadModel(result));
	}

	return models;
}

void MySQLDatabase::RemoveUnreferencedUgcModels() {
	ExecuteDelete("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents WHERE ugc_id IS NOT NULL);");
}

void MySQLDatabase::InsertNewUgcModel(
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

void MySQLDatabase::DeleteUgcModelData(const LWOOBJID& modelId) {
	ExecuteDelete("DELETE FROM ugc WHERE id = ?;", modelId);
	ExecuteDelete("DELETE FROM properties_contents WHERE ugc_id = ?;", modelId);
}

void MySQLDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::stringstream& lxfml) {
	const std::istream stream(lxfml.rdbuf());
	ExecuteUpdate("UPDATE ugc SET lxfml = ? WHERE id = ?;", &stream, modelId);
}

std::optional<IUgc::Model> MySQLDatabase::GetUgcModel(const LWOOBJID ugcId) {
	auto result = ExecuteSelect("SELECT u.id AS ugcID, lxfml, pc.id AS modelID FROM ugc AS u JOIN properties_contents AS pc ON pc.ugc_id = u.id WHERE u.id = ?", ugcId);

	std::optional<IUgc::Model> toReturn = std::nullopt;
	if (result->next()) {
		toReturn = ReadModel(result);
	}

	return toReturn;
}
