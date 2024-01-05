#include "CDPetComponentTable.h"

namespace {
	// Default entries for fallback
	CDPetComponent defaultEntry {
		.id = static_cast<unsigned int>(-1),
		UNUSED_DEFAULT(.minTameUpdateTime = 60.0f,)
		UNUSED_DEFAULT(.maxTameUpdateTime = 300.0f,)
		UNUSED_DEFAULT(.percentTameChance = 101.0f,)
		UNUSED_DEFAULT(.tameability = 100.0f,)
		UNUSED_DEFAULT(.elementType = 1,)
		.walkSpeed = 2.5f,
		.runSpeed = 5.0f,
		.sprintSpeed = 10.0f,
		UNUSED_DEFAULT(.idleTimeMin = 60.0f,)
		UNUSED_DEFAULT(.idleTimeMax = 300.0f,)
		UNUSED_DEFAULT(.petForm = 0,)
		.imaginationDrainRate = 60.0f,
		UNUSED_DEFAULT(.AudioMetaEventSet = "",)
		UNUSED_DEFAULT(.buffIDs = "",)
	};
}

void CDPetComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PetComponent");
	while (!tableData.eof()) {
		CDPetComponent entry;
		entry.id = tableData.getIntField("id", defaultEntry.id);
		UNUSED_COLUMN(entry.minTameUpdateTime = tableData.getFloatField("minTameUpdateTime", defaultEntry.minTameUpdateTime);)
		UNUSED_COLUMN(entry.maxTameUpdateTime = tableData.getFloatField("maxTameUpdateTime", defaultEntry.maxTameUpdateTime);)
		UNUSED_COLUMN(entry.percentTameChance = tableData.getFloatField("percentTameChance", defaultEntry.percentTameChance);)
		UNUSED_COLUMN(entry.tameability = tableData.getFloatField("tamability", defaultEntry.tameability);) // Mispelled as "tamability" in CDClient
		UNUSED_COLUMN(entry.elementType = tableData.getIntField("elementType", defaultEntry.elementType);)
		entry.walkSpeed = tableData.getFloatField("walkSpeed", defaultEntry.walkSpeed);
		entry.runSpeed = tableData.getFloatField("runSpeed", defaultEntry.runSpeed);
		entry.sprintSpeed = tableData.getFloatField("sprintSpeed", defaultEntry.sprintSpeed);
		UNUSED_COLUMN(entry.idleTimeMin = tableData.getFloatField("idleTimeMin", defaultEntry.idleTimeMin);)
		UNUSED_COLUMN(entry.idleTimeMax = tableData.getFloatField("idleTimeMax", defaultEntry.idleTimeMax);)
		UNUSED_COLUMN(entry.petForm = tableData.getIntField("petForm", 0);)
		entry.imaginationDrainRate = tableData.getFloatField("imaginationDrainRate", defaultEntry.imaginationDrainRate);
		UNUSED_COLUMN(entry.AudioMetaEventSet = tableData.getStringField("AudioMetaEventSet", defaultEntry.AudioMetaEventSet);)
		UNUSED_COLUMN(entry.buffIDs = tableData.getStringField("buffIDs", defaultEntry.buffIDs);)

		m_entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();

}

CDPetComponent& CDPetComponentTable::GetByID(unsigned int componentID) {
	auto itr = m_entries.find(componentID);
	if (itr == m_entries.end()) {
		LOG("Unable to load pet component (ID %i) values from database! Using default values instead.", componentID);
		return defaultEntry;
	}
	return itr->second;
}
