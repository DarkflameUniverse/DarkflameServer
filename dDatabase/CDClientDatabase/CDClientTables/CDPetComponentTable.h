#pragma once
#include "CDTable.h"
#include <string>

struct CDPetComponent {
	unsigned int id;
	UNUSED_COLUMN(float minTameUpdateTime;)
	UNUSED_COLUMN(float maxTameUpdateTime;)
	UNUSED_COLUMN(float percentTameChance;)
	UNUSED_COLUMN(float tameability;) // Mispelled as "tamability" in CDClient
	UNUSED_COLUMN(unsigned int elementType;)
	float walkSpeed;
	float runSpeed;
	float sprintSpeed;
	UNUSED_COLUMN(float idleTimeMin;)
	UNUSED_COLUMN(float idleTimeMax;)
	UNUSED_COLUMN(unsigned int petForm;)
	float imaginationDrainRate;
	UNUSED_COLUMN(std::string AudioMetaEventSet;)
	UNUSED_COLUMN(std::string buffIDs;)
};

class CDPetComponentTable : public CDTable<CDPetComponentTable> {
public:
	void LoadValuesFromDatabase();

	static const std::string GetTableName() { return "PetComponent"; };
	CDPetComponent* GetByID(unsigned int componentID);
	
private:
	std::map<unsigned int, CDPetComponent> m_entries;
};
