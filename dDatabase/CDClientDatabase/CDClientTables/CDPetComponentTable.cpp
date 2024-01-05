#include "CDPetComponentTable.h"

void CDPetComponentTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PetComponent");
	while (!tableData.eof()) {
		CDPetComponent entry;
		entry.id = tableData.getIntField("id", -1);
		UNUSED_COLUMN(entry.minTameUpdateTime = tableData.getFloatField("minTameUpdateTime", 60.0f);)
		UNUSED_COLUMN(entry.maxTameUpdateTime = tableData.getFloatField("maxTameUpdateTime", 300.0f);)
		UNUSED_COLUMN(entry.percentTameChance = tableData.getFloatField("percentTameChance", 101.0f);)
		UNUSED_COLUMN(entry.tameability = tableData.getFloatField("tamability", 100.0f);) // Mispelled as "tamability" in CDClient
		UNUSED_COLUMN(entry.elementType = tableData.getIntField("elementType", 1);)
		entry.walkSpeed = tableData.getFloatField("walkSpeed", 2.5f);
		entry.runSpeed = tableData.getFloatField("runSpeed", 5.0f);
		entry.sprintSpeed = tableData.getFloatField("sprintSpeed", 10.0f);
		UNUSED_COLUMN(entry.idleTimeMin = tableData.getFloatField("idleTimeMin", 60.0f);)
		UNUSED_COLUMN(entry.idleTimeMax = tableData.getFloatField("idleTimeMax", 300.0f);)
		UNUSED_COLUMN(entry.petForm = tableData.getIntField("petForm", 0);)
		entry.imaginationDrainRate = tableData.getFloatField("imaginationDrainRate", 60.0f);
		UNUSED_COLUMN(entry.AudioMetaEventSet = tableData.getStringField("AudioMetaEventSet", "");)
		UNUSED_COLUMN(entry.buffIDs = tableData.getStringField("buffIDs", "");)

		m_entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();

}

CDPetComponent& CDPetComponentTable::GetByID(unsigned int componentID) {
	auto itr = m_entries.find(componentID);
	if (itr == m_entries.end()) throw std::exception(); // TODO: Use a default set of values instead?
	return itr->second;
}
