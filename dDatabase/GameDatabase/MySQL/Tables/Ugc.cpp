#include "MySQLDatabase.h"

std::vector<IUgc::Model> MySQLDatabase::GetUgcModels(const LWOOBJID& propertyId) {
	auto result = ExecuteSelect(
		"SELECT lxfml, u.id FROM ugc AS u JOIN properties_contents AS pc ON u.id = pc.ugc_id WHERE lot = 14 AND property_id = ? AND pc.ugc_id IS NOT NULL;",
		propertyId);

	std::vector<IUgc::Model> toReturn;

	while (result->next()) {
		IUgc::Model model;

		// blob is owned by the query, so we need to do a deep copy :/
		std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
		model.lxfmlData << blob->rdbuf();
		model.id = result->getUInt64("id");
		toReturn.push_back(std::move(model));
	}

	return toReturn;
}

std::vector<IUgc::Model> MySQLDatabase::GetAllUgcModels() {
	auto result = ExecuteSelect("SELECT id, lxfml FROM ugc;");

	std::vector<IUgc::Model> models;
	models.reserve(result->rowsCount());
	while (result->next()) {
		IUgc::Model model;
		model.id = result->getInt64("id");

		// blob is owned by the query, so we need to do a deep copy :/
		std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
		model.lxfmlData << blob->rdbuf();
		models.push_back(std::move(model));
	}

	return models;
}

void MySQLDatabase::RemoveUnreferencedUgcModels() {
	ExecuteDelete("DELETE FROM ugc WHERE id NOT IN (SELECT ugc_id FROM properties_contents WHERE ugc_id IS NOT NULL);");
}

void MySQLDatabase::InsertNewUgcModel(
	std::istringstream& sd0Data, // cant be const sad
	const uint32_t blueprintId,
	const uint32_t accountId,
	const uint32_t characterId) {
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

void MySQLDatabase::UpdateUgcModelData(const LWOOBJID& modelId, std::istringstream& lxfml) {
	const std::istream stream(lxfml.rdbuf());
	ExecuteUpdate("UPDATE ugc SET lxfml = ? WHERE id = ?;", &stream, modelId);
}
