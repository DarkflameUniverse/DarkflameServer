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
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		const uint32_t componentID = tableData.getIntField("id", defaultEntry.id);

		auto& entry = entries[componentID];

		entry.id = componentID;
		UNUSED_COLUMN(entry.minTameUpdateTime = tableData.getFloatField("minTameUpdateTime", defaultEntry.minTameUpdateTime));
		UNUSED_COLUMN(entry.maxTameUpdateTime = tableData.getFloatField("maxTameUpdateTime", defaultEntry.maxTameUpdateTime));
		UNUSED_COLUMN(entry.percentTameChance = tableData.getFloatField("percentTameChance", defaultEntry.percentTameChance));
		UNUSED_COLUMN(entry.tameability = tableData.getFloatField("tamability", defaultEntry.tameability)); // Mispelled as "tamability" in CDClient
		UNUSED_COLUMN(entry.elementType = tableData.getIntField("elementType", defaultEntry.elementType));
		entry.walkSpeed = static_cast<float>(tableData.getFloatField("walkSpeed", defaultEntry.walkSpeed));
		entry.runSpeed = static_cast<float>(tableData.getFloatField("runSpeed", defaultEntry.runSpeed));
		entry.sprintSpeed = static_cast<float>(tableData.getFloatField("sprintSpeed", defaultEntry.sprintSpeed));
		UNUSED_COLUMN(entry.idleTimeMin = tableData.getFloatField("idleTimeMin", defaultEntry.idleTimeMin));
		UNUSED_COLUMN(entry.idleTimeMax = tableData.getFloatField("idleTimeMax", defaultEntry.idleTimeMax));
		UNUSED_COLUMN(entry.petForm = tableData.getIntField("petForm", defaultEntry.petForm));
		entry.imaginationDrainRate = static_cast<float>(tableData.getFloatField("imaginationDrainRate", defaultEntry.imaginationDrainRate));
		UNUSED_COLUMN(entry.AudioMetaEventSet = tableData.getStringField("AudioMetaEventSet", defaultEntry.AudioMetaEventSet));
		UNUSED_COLUMN(entry.buffIDs = tableData.getStringField("buffIDs", defaultEntry.buffIDs));

		tableData.nextRow();
	}
}

void CDPetComponentTable::LoadValuesFromDefaults() {
	GetEntriesMutable().insert(std::make_pair(defaultEntry.id, defaultEntry));
}

CDPetComponent& CDPetComponentTable::GetByID(const uint32_t componentID) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(componentID);
	if (itr == entries.end()) {
		LOG("Unable to load pet component (ID %i) values from database! Using default values instead.", componentID);
		return defaultEntry;
	}
	return itr->second;
}
