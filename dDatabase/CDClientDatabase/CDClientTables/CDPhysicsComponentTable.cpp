#include "CDPhysicsComponentTable.h"

void CDPhysicsComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PhysicsComponent");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		const uint32_t componentID = tableData.getIntField("id", -1);

		auto& entry = entries[componentID];
		entry.id = componentID;
		entry.bStatic = tableData.getIntField("static", -1) != 0;
		entry.physicsAsset = tableData.getStringField("physics_asset", "");
		UNUSED_COLUMN(entry.jump = tableData.getIntField("jump", -1) != 0;)
		UNUSED_COLUMN(entry.doubleJump = tableData.getIntField("doublejump", -1) != 0;)
		entry.speed = static_cast<float>(tableData.getFloatField("speed", -1));
		UNUSED_COLUMN(entry.rotSpeed = tableData.getFloatField("rotSpeed", -1);)
		entry.playerHeight = static_cast<float>(tableData.getFloatField("playerHeight"));
		entry.playerRadius = static_cast<float>(tableData.getFloatField("playerRadius"));
		entry.pcShapeType = tableData.getIntField("pcShapeType");
		entry.collisionGroup = tableData.getIntField("collisionGroup");
		UNUSED_COLUMN(entry.airSpeed = tableData.getFloatField("airSpeed");)
		UNUSED_COLUMN(entry.boundaryAsset = tableData.getStringField("boundaryAsset");)
		UNUSED_COLUMN(entry.jumpAirSpeed = tableData.getFloatField("jumpAirSpeed");)
		UNUSED_COLUMN(entry.friction = tableData.getFloatField("friction");)
		UNUSED_COLUMN(entry.gravityVolumeAsset = tableData.getStringField("gravityVolumeAsset");)

		tableData.nextRow();
	}
}

CDPhysicsComponent* CDPhysicsComponentTable::GetByID(const uint32_t componentID) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(componentID);
	return itr != entries.end() ? &itr->second : nullptr;
}

