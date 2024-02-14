#include "CDPhysicsComponentTable.h"

void CDPhysicsComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PhysicsComponent");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDPhysicsComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.bStatic = tableData.getIntField("static", -1) != 0;
		entry.physicsAsset = tableData.getStringField("physics_asset", "");
		UNUSED(entry->jump = tableData.getIntField("jump", -1) != 0);
		UNUSED(entry->doublejump = tableData.getIntField("doublejump", -1) != 0);
		entry.speed = tableData.getFloatField("speed", -1);
		UNUSED(entry->rotSpeed = tableData.getFloatField("rotSpeed", -1));
		entry.playerHeight = tableData.getFloatField("playerHeight");
		entry.playerRadius = tableData.getFloatField("playerRadius");
		entry.pcShapeType = tableData.getIntField("pcShapeType");
		entry.collisionGroup = tableData.getIntField("collisionGroup");
		UNUSED(entry->airSpeed = tableData.getFloatField("airSpeed"));
		UNUSED(entry->boundaryAsset = tableData.getStringField("boundaryAsset"));
		UNUSED(entry->jumpAirSpeed = tableData.getFloatField("jumpAirSpeed"));
		UNUSED(entry->friction = tableData.getFloatField("friction"));
		UNUSED(entry->gravityVolumeAsset = tableData.getStringField("gravityVolumeAsset"));

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPhysicsComponent* CDPhysicsComponentTable::GetByID(uint32_t componentID) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(componentID);
	return itr != entries.end() ? &itr->second : nullptr;
}

