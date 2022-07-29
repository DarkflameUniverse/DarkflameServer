#include "CDZoneTableTable.h"

//! Constructor
CDZoneTableTable::CDZoneTableTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ZoneTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ZoneTable");
	while (!tableData.eof()) {
		CDZoneTable entry;
		entry.zoneID = tableData.getIntField(0, -1);
		entry.locStatus = tableData.getIntField(1, -1);
		entry.zoneName = tableData.getStringField(2, "");
		entry.scriptID = tableData.getIntField(3, -1);
		entry.ghostdistance_min = tableData.getFloatField(4, -1.0f);
		entry.ghostdistance = tableData.getFloatField(5, -1.0f);
		entry.population_soft_cap = tableData.getIntField(6, -1);
		entry.population_hard_cap = tableData.getIntField(7, -1);
		UNUSED(entry.DisplayDescription = tableData.getStringField(8, ""));
		UNUSED(entry.mapFolder = tableData.getStringField(9, ""));
		entry.smashableMinDistance = tableData.getFloatField(10, -1.0f);
		entry.smashableMaxDistance = tableData.getFloatField(11, -1.0f);
		UNUSED(entry.mixerProgram = tableData.getStringField(12, ""));
		UNUSED(entry.clientPhysicsFramerate = tableData.getStringField(13, ""));
		UNUSED(entry.serverPhysicsFramerate = tableData.getStringField(14, ""));
		entry.zoneControlTemplate = tableData.getIntField(15, -1);
		entry.widthInChunks = tableData.getIntField(16, -1);
		entry.heightInChunks = tableData.getIntField(17, -1);
		entry.petsAllowed = tableData.getIntField(18, -1) == 1 ? true : false;
		entry.localize = tableData.getIntField(19, -1) == 1 ? true : false;
		entry.fZoneWeight = tableData.getFloatField(20, -1.0f);
		UNUSED(entry.thumbnail = tableData.getStringField(21, ""));
		entry.PlayerLoseCoinsOnDeath = tableData.getIntField(22, -1) == 1 ? true : false;
		UNUSED(entry.disableSaveLoc = tableData.getIntField(23, -1) == 1 ? true : false);
		entry.teamRadius = tableData.getFloatField(24, -1.0f);
		UNUSED(entry.gate_version = tableData.getStringField(25, ""));
		UNUSED(entry.mountsAllowed = tableData.getIntField(26, -1) == 1 ? true : false);

		this->m_Entries.insert(std::make_pair(entry.zoneID, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDZoneTableTable::~CDZoneTableTable(void) {}

//! Returns the table's name
std::string CDZoneTableTable::GetName(void) const {
	return "ZoneTable";
}

//! Queries the table with a zoneID to find.
const CDZoneTable* CDZoneTableTable::Query(unsigned int zoneID) {
	const auto& iter = m_Entries.find(zoneID);

	if (iter != m_Entries.end()) {
		return &iter->second;
	}

	return nullptr;
}
