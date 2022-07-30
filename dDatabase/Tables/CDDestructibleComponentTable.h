#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDDestructibleComponentTable.hpp
 \brief Contains data for the DestructibleComponent table
 */

 //! ItemComponent Struct
struct CDDestructibleComponent {
	unsigned int id;                            //!< The component ID from the ComponentsRegistry Table
	int faction;                       //!< The Faction ID of the object
	std::string factionList;               //!< A list of the faction IDs
	int life;                          //!< The amount of life of the object
	unsigned int imagination;                   //!< The amount of imagination of the object
	int LootMatrixIndex;               //!< The Loot Matrix Index
	int CurrencyIndex;                 //!< The Currency Index
	unsigned int level;                         //!< ???
	float armor;                        //!< The amount of armor of the object
	unsigned int death_behavior;                //!< The behavior ID of the death behavior
	bool isnpc;                         //!< Whether or not the object is an NPC
	unsigned int attack_priority;               //!< ???
	bool isSmashable;                   //!< Whether or not the object is smashable
	int difficultyLevel;               //!< ???
};

//! ItemComponent table
class CDDestructibleComponentTable : public CDTable {
private:
	std::vector<CDDestructibleComponent> entries;

public:

	//! Constructor
	CDDestructibleComponentTable(void);

	//! Destructor
	~CDDestructibleComponentTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDDestructibleComponent> Query(std::function<bool(CDDestructibleComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDDestructibleComponent> GetEntries(void) const;

};
