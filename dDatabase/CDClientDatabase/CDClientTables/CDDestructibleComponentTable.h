#pragma once

// Custom Classes
#include "CDTable.h"

struct CDDestructibleComponent {
	uint32_t id;                            //!< The component ID from the ComponentsRegistry Table
	int32_t faction;                       //!< The Faction ID of the object
	std::string factionList;               //!< A list of the faction IDs
	int32_t life;                          //!< The amount of life of the object
	uint32_t imagination;                   //!< The amount of imagination of the object
	int32_t LootMatrixIndex;               //!< The Loot Matrix Index
	int32_t CurrencyIndex;                 //!< The Currency Index
	uint32_t level;                         //!< ???
	float armor;                        //!< The amount of armor of the object
	uint32_t death_behavior;                //!< The behavior ID of the death behavior
	bool isnpc;                         //!< Whether or not the object is an NPC
	uint32_t attack_priority;               //!< ???
	bool isSmashable;                   //!< Whether or not the object is smashable
	int32_t difficultyLevel;               //!< ???
};

class CDDestructibleComponentTable : public CDTable<CDDestructibleComponentTable> {
private:
	std::vector<CDDestructibleComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDDestructibleComponent> Query(std::function<bool(CDDestructibleComponent)> predicate);

	const std::vector<CDDestructibleComponent>& GetEntries(void) const;
};
