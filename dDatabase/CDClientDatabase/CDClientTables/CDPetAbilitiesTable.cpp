#include "CDPetAbilitiesTable.h"
#include "ePetAbilityType.h"

namespace {
	// Default entries for fallback
	CDPetAbilities defaultEntry{
		.id = ePetAbilityType::Invalid,
		UNUSED_ENTRY(.AbilityName = "invalid",)
		.imaginationCost = 0,
		UNUSED_ENTRY(.locStatus = 2,)
	};
}

void CDPetAbilitiesTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM PetAbilities");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		const auto abilityId =
			static_cast<ePetAbilityType>(tableData.getIntField("id", static_cast<int>(defaultEntry.id)));
		auto& entry = entries[abilityId];

		entry.id = abilityId;
		UNUSED_COLUMN(entry.abilityName = tableData.getStringField("AbilityName", defaultEntry.abilityName));
		entry.imaginationCost = tableData.getIntField("ImaginationCost", defaultEntry.imaginationCost);
		UNUSED_COLUMN(entry.locStatus = tableData.getIntField("locStatus", defaultEntry.locStatus));

		tableData.nextRow();
	}
}

void CDPetAbilitiesTable::LoadValuesFromDefaults() {
	GetEntriesMutable().emplace(defaultEntry.id, defaultEntry);
}

const CDPetAbilities& CDPetAbilitiesTable::GetByID(const ePetAbilityType id) {
	const auto& entries = GetEntries();
	const auto itr = entries.find(id);
	if (itr == entries.cend()) {
		LOG("Unable to load pet ability (ID %i) values from database! Using default values instead.", id);
		return defaultEntry;
	}
	return itr->second;
}
