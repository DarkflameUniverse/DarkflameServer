#pragma once
#include "CDTable.h"
#include <cstdint>
#include <string>

// Forward declarations
enum class ePetAbilityType : uint32_t;

struct CDPetAbilities {
	ePetAbilityType id;
	UNUSED_COLUMN(std::string abilityName;)
	int32_t imaginationCost;
	UNUSED_COLUMN(uint32_t locStatus;)
};

class CDPetAbilitiesTable : public CDTable<CDPetAbilitiesTable, std::map<ePetAbilityType, CDPetAbilities>> {
public:

	/**
	 * Load values from the CD client database
	*/
	void LoadValuesFromDatabase();

	/**
	 * Load the default values into memory instead of attempting to connect to the CD client database
	*/
	void LoadValuesFromDefaults();

	/**
	 * Gets the pet ability table corresponding to the pet ability ID
	 * @returns A pointer to the corresponding table, or nullptr if one could not be found
	*/
	const CDPetAbilities& GetByID(const ePetAbilityType id);
};
