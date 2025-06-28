#include "ModelNormalizeMigration.h"

#include "Database.h"
#include "Lxfml.h"
#include "Sd0.h"

void ModelNormalizeMigration::Run() {
	const auto oldCommit = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);
	for (auto& [lxfmlData, id, modelID] : Database::Get()->GetAllUgcModels()) {
		const auto model = Database::Get()->GetModel(modelID);
		// only BBB models (lot 14) and models with a position of NiPoint3::ZERO need to have their position fixed.
		if (model.position != NiPoint3Constant::ZERO || model.lot != 14) continue;
	
		Sd0 sd0(lxfmlData);
		const auto asStr = sd0.GetAsStringUncompressed();
		const auto [newLxfml, newCenter] = Lxfml::NormalizePositionOnlyFirstPart(asStr);
		if (newCenter == NiPoint3Constant::ZERO) {
			LOG("Failed to update model %llu due to failure reading xml.");
			continue;
		}

		LOG("Updated model %llu to have a center of %f %f %f", modelID, newCenter.x, newCenter.y, newCenter.z);
		sd0.FromData(reinterpret_cast<const uint8_t*>(newLxfml.data()), newLxfml.size());
		auto asStream = sd0.GetAsStream();
		Database::Get()->UpdateModel(model.id, newCenter, model.rotation, model.behaviors);
		Database::Get()->UpdateUgcModelData(id, asStream);
	}
	Database::Get()->SetAutoCommit(oldCommit);
}

void ModelNormalizeMigration::RunAfterFirstPart() {
	const auto oldCommit = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);
	for (auto& [lxfmlData, id, modelID] : Database::Get()->GetAllUgcModels()) {
		const auto model = Database::Get()->GetModel(modelID);
		// only BBB models (lot 14) need to have their position fixed from the above blunder
		if (model.lot != 14) continue;
	
		Sd0 sd0(lxfmlData);
		const auto asStr = sd0.GetAsStringUncompressed();
		const auto [newLxfml, newCenter] = Lxfml::NormalizePositionAfterFirstPart(asStr, model.position);

		sd0.FromData(reinterpret_cast<const uint8_t*>(newLxfml.data()), newLxfml.size());
		auto asStream = sd0.GetAsStream();
		Database::Get()->UpdateModel(model.id, newCenter, model.rotation, model.behaviors);
		Database::Get()->UpdateUgcModelData(id, asStream);
	}
	Database::Get()->SetAutoCommit(oldCommit);
}

void ModelNormalizeMigration::RunBrickBuildGrid() {
	const auto oldCommit = Database::Get()->GetAutoCommit();
	Database::Get()->SetAutoCommit(false);
	for (auto& [lxfmlData, id, modelID] : Database::Get()->GetAllUgcModels()) {
		const auto model = Database::Get()->GetModel(modelID);
		// only BBB models (lot 14) need to have their position fixed from the above blunder
		if (model.lot != 14) continue;
	
		Sd0 sd0(lxfmlData);
		const auto asStr = sd0.GetAsStringUncompressed();
		const auto [newLxfml, newCenter] = Lxfml::NormalizePosition(asStr, model.position);

		sd0.FromData(reinterpret_cast<const uint8_t*>(newLxfml.data()), newLxfml.size());
		LOG("Updated model %llu to have a center of %f %f %f", modelID, newCenter.x, newCenter.y, newCenter.z);
		auto asStream = sd0.GetAsStream();
		Database::Get()->UpdateModel(model.id, newCenter, model.rotation, model.behaviors);
		Database::Get()->UpdateUgcModelData(id, asStream);
	}
	Database::Get()->SetAutoCommit(oldCommit);
}
