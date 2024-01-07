#include "CDPetComponentTable.h"

namespace {
	// Default entries for fallback
	CDPetComponent defaultEntry{
		.id = 0,
		UNUSED_ENTRY(.minTameUpdateTime = 60.0f,)
		UNUSED_ENTRY(.maxTameUpdateTime = 300.0f,)
		UNUSED_ENTRY(.percentTameChance = 101.0f,)
		UNUSED_ENTRY(.tameability = 100.0f,)
		UNUSED_ENTRY(.elementType = 1,)
		.walkSpeed = 2.5f,
		.runSpeed = 5.0f,
		.sprintSpeed = 10.0f,
		UNUSED_ENTRY(.idleTimeMin = 60.0f,)
		UNUSED_ENTRY(.idleTimeMax = 300.0f,)
		UNUSED_ENTRY(.petForm = 0,)
		.imaginationDrainRate = 60.0f,
		UNUSED_ENTRY(.AudioMetaEventSet = "",)
		UNUSED_ENTRY(.buffIDs = "",)
	};
}

void CDPetComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PetComponent");
	while (!tableData.eof()) {
		const uint32_t componentID = tableData.getIntField("id", defaultEntry.id);

		m_Entries.try_emplace(
			componentID,
			componentID,
			UNUSED_ENTRY(tableData.getFloatField("minTameUpdateTime", defaultEntry.minTameUpdateTime),)
			UNUSED_ENTRY(tableData.getFloatField("maxTameUpdateTime", defaultEntry.maxTameUpdateTime),)
			UNUSED_ENTRY(tableData.getFloatField("percentTameChance", defaultEntry.percentTameChance),)
			UNUSED_ENTRY(tableData.getFloatField("tamability", defaultEntry.tameability),) // Mispelled as "tamability" in CDClient
			UNUSED_ENTRY(tableData.getIntField("elementType", defaultEntry.elementType),)
			static_cast<float>(tableData.getFloatField("walkSpeed", defaultEntry.walkSpeed)),
			static_cast<float>(tableData.getFloatField("runSpeed", defaultEntry.runSpeed)),
			static_cast<float>(tableData.getFloatField("sprintSpeed", defaultEntry.sprintSpeed)),
			UNUSED_ENTRY(tableData.getFloatField("idleTimeMin", defaultEntry.idleTimeMin),)
			UNUSED_ENTRY(tableData.getFloatField("idleTimeMax", defaultEntry.idleTimeMax),)
			UNUSED_ENTRY(tableData.getIntField("petForm", 0),)
			static_cast<float>(tableData.getFloatField("imaginationDrainRate", defaultEntry.imaginationDrainRate))
			UNUSED_ENTRY(tableData.getStringField("AudioMetaEventSet", defaultEntry.AudioMetaEventSet),)
			UNUSED_ENTRY(tableData.getStringField("buffIDs", defaultEntry.buffIDs),)
		);

		tableData.nextRow();
	}
}

void CDPetComponentTable::LoadValuesFromDefaults() {
	m_Entries.insert(std::make_pair(defaultEntry.id, defaultEntry));
}

CDPetComponent& CDPetComponentTable::GetByID(const uint32_t componentID) {
	auto itr = m_Entries.find(componentID);
	if (itr == m_Entries.end()) {
		LOG("Unable to load pet component (ID %i) values from database! Using default values instead.", componentID);
		return defaultEntry;
	}
	return itr->second;
}
