#include "CDPhysicsComponentTable.h"

CDPhysicsComponentTable::CDPhysicsComponentTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PhysicsComponent");
	while (!tableData.eof()) {
		CDPhysicsComponent* entry = new CDPhysicsComponent();
		entry->id = tableData.getIntField("id", -1);
		entry->bStatic = tableData.getIntField("static", -1) != 0;
		entry->physicsAsset = tableData.getStringField("physics_asset", "");
		UNUSED(entry->jump = tableData.getIntField("jump", -1) != 0);
		UNUSED(entry->doublejump = tableData.getIntField("doublejump", -1) != 0);
		entry->speed = tableData.getFloatField("speed", -1);
		UNUSED(entry->rotSpeed = tableData.getFloatField("rotSpeed", -1));
		entry->playerHeight = tableData.getFloatField("playerHeight");
		entry->playerRadius = tableData.getFloatField("playerRadius");
		entry->pcShapeType = tableData.getIntField("pcShapeType");
		entry->collisionGroup = tableData.getIntField("collisionGroup");
		UNUSED(entry->airSpeed = tableData.getFloatField("airSpeed"));
		UNUSED(entry->boundaryAsset = tableData.getStringField("boundaryAsset"));
		UNUSED(entry->jumpAirSpeed = tableData.getFloatField("jumpAirSpeed"));
		UNUSED(entry->friction = tableData.getFloatField("friction"));
		UNUSED(entry->gravityVolumeAsset = tableData.getStringField("gravityVolumeAsset"));

		m_entries.insert(std::make_pair(entry->id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

CDPhysicsComponentTable::~CDPhysicsComponentTable(void) {
	for (auto e : m_entries) {
		if (e.second) delete e.second;
	}

	m_entries.clear();
}

std::string CDPhysicsComponentTable::GetName(void) const {
	return "PhysicsComponent";
}

CDPhysicsComponent* CDPhysicsComponentTable::GetByID(unsigned int componentID) {
	for (auto e : m_entries) {
		if (e.first == componentID) return e.second;
	}

	return nullptr;
}

