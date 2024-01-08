#include "CDPhysicsComponentTable.h"

void CDPhysicsComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PhysicsComponent");
	while (!tableData.eof()) {
		const uint32_t componentID = tableData.getIntField("id", -1);

		auto& entry = m_Entries[componentID];
		entry.id = componentID,
		entry.bStatic = tableData.getIntField("static", -1) != 0,
		entry.physicsAsset = tableData.getStringField("physics_asset", ""),
		UNUSED_ENTRY(entry.jump = tableData.getIntField("jump", -1) != 0,)
		UNUSED_ENTRY(entry.doubleJump = tableData.getIntField("doublejump", -1) != 0,)
		entry.speed = static_cast<float>(tableData.getFloatField("speed", -1)),
		UNUSED_ENTRY(entry.rotSpeed = tableData.getFloatField("rotSpeed", -1),)
		entry.playerHeight = static_cast<float>(tableData.getFloatField("playerHeight")),
		entry.playerRadius = static_cast<float>(tableData.getFloatField("playerRadius")),
		entry.pcShapeType = tableData.getIntField("pcShapeType"),
		entry.collisionGroup = tableData.getIntField("collisionGroup"),
		UNUSED_ENTRY(entry.airSpeed = tableData.getFloatField("airSpeed"),)
		UNUSED_ENTRY(entry.boundaryAsset = tableData.getStringField("boundaryAsset"),)
		UNUSED_ENTRY(entry.jumpAirSpeed = tableData.getFloatField("jumpAirSpeed"),)
		UNUSED_ENTRY(entry.friction = tableData.getFloatField("friction"),)
		UNUSED_ENTRY(entry.gravityVolumeAsset = tableData.getStringField("gravityVolumeAsset"),)

		tableData.nextRow();
	}
}

CDPhysicsComponent* CDPhysicsComponentTable::GetByID(const uint32_t componentID) {
	auto itr = m_Entries.find(componentID);
	return itr != m_Entries.end() ? &itr->second : nullptr;
}

