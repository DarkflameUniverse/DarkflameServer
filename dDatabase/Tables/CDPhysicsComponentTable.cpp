#include "CDPhysicsComponentTable.h"

CDPhysicsComponentTable::CDPhysicsComponentTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PhysicsComponent");
	while (!tableData.eof()) {
		CDPhysicsComponent* entry = new CDPhysicsComponent();
		entry->id = tableData.getIntField(0, -1);
		entry->bStatic = tableData.getIntField(1, -1) != 0;
		entry->physicsAsset = tableData.getStringField(2, "");
		UNUSED(entry->jump = tableData.getIntField(3, -1) != 0);
		UNUSED(entry->doublejump = tableData.getIntField(4, -1) != 0);
		entry->speed = tableData.getFloatField(5, -1);
		UNUSED(entry->rotSpeed = tableData.getFloatField(6, -1));
		entry->playerHeight = tableData.getFloatField(7);
		entry->playerRadius = tableData.getFloatField(8);
		entry->pcShapeType = tableData.getIntField(9);
		entry->collisionGroup = tableData.getIntField(10);
		UNUSED(entry->airSpeed = tableData.getFloatField(11));
		UNUSED(entry->boundaryAsset = tableData.getStringField(12));
		UNUSED(entry->jumpAirSpeed = tableData.getFloatField(13));
		UNUSED(entry->friction = tableData.getFloatField(14));
		UNUSED(entry->gravityVolumeAsset = tableData.getStringField(15));

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
