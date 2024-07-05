#include "MySQLDatabase.h"

std::vector<IPropertyContents::Model> MySQLDatabase::GetPropertyModels(const LWOOBJID& propertyId) {
	auto result = ExecuteSelect(
		"SELECT id, lot, x, y, z, rx, ry, rz, rw, ugc_id, "
		"behavior_1, behavior_2, behavior_3, behavior_4, behavior_5 "
		"FROM properties_contents WHERE property_id = ?;", propertyId);

	std::vector<IPropertyContents::Model> toReturn;
	toReturn.reserve(result->rowsCount());
	while (result->next()) {
		IPropertyContents::Model model;
		model.id = result->getUInt64("id");
		model.lot = static_cast<LOT>(result->getUInt("lot"));
		model.position.x = result->getFloat("x");
		model.position.y = result->getFloat("y");
		model.position.z = result->getFloat("z");
		model.rotation.w = result->getFloat("rw");
		model.rotation.x = result->getFloat("rx");
		model.rotation.y = result->getFloat("ry");
		model.rotation.z = result->getFloat("rz");
		model.ugcId = result->getUInt64("ugc_id");
		model.behaviors[0] = result->getInt("behavior_1");
		model.behaviors[1] = result->getInt("behavior_2");
		model.behaviors[2] = result->getInt("behavior_3");
		model.behaviors[3] = result->getInt("behavior_4");
		model.behaviors[4] = result->getInt("behavior_5");

		toReturn.push_back(std::move(model));
	}
	return toReturn;
}

void MySQLDatabase::InsertNewPropertyModel(const LWOOBJID& propertyId, const IPropertyContents::Model& model, const std::string_view name) {
	try {
		ExecuteInsert(
			"INSERT INTO properties_contents"
			"(id, property_id, ugc_id, lot, x, y, z, rx, ry, rz, rw, model_name, model_description, behavior_1, behavior_2, behavior_3, behavior_4, behavior_5)"
			"VALUES (?,  ?,           ?,      ?,   ?, ?, ?, ?,  ?,  ?,  ?,  ?,    ?,           ?,          ?,          ?,          ?,          ?)",
			//       1,  2,           3,      4,   5, 6, 7, 8,  9,  10, 11, 12,   13,          14,         15,         16,         17          18
			model.id, propertyId, model.ugcId == 0 ? std::nullopt : std::optional(model.ugcId), static_cast<uint32_t>(model.lot),
			model.position.x, model.position.y, model.position.z, model.rotation.x, model.rotation.y, model.rotation.z, model.rotation.w,
			name, "", // Model description.  TODO implement this.
			model.behaviors[0], // behavior 1
			model.behaviors[1], // behavior 2
			model.behaviors[2], // behavior 3
			model.behaviors[3], // behavior 4
			model.behaviors[4] // behavior 5
		);
	} catch (sql::SQLException& e) {
		LOG("Error inserting new property model: %s", e.what());
	}
}

void MySQLDatabase::UpdateModel(const LWOOBJID& propertyId, const NiPoint3& position, const NiQuaternion& rotation, const std::array<std::pair<int32_t, std::string>, 5>& behaviors) {
	ExecuteUpdate(
		"UPDATE properties_contents SET x = ?, y = ?, z = ?, rx = ?, ry = ?, rz = ?, rw = ?, "
		"behavior_1 = ?, behavior_2 = ?, behavior_3 = ?, behavior_4 = ?, behavior_5 = ? WHERE id = ?;",
		position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, rotation.w,
		behaviors[0].first, behaviors[1].first, behaviors[2].first, behaviors[3].first, behaviors[4].first, propertyId);
}

void MySQLDatabase::RemoveModel(const LWOOBJID& modelId) {
	ExecuteDelete("DELETE FROM properties_contents WHERE id = ?;", modelId);
}
