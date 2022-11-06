#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDLootTableTable.hpp
 \brief Contains data for the LootTable table
 */

 //! LootTable Struct
struct CDLootTable {
	unsigned int itemid;                 //!< The LOT of the item
	unsigned int LootTableIndex;         //!< The Loot Table Index
	unsigned int id;                     //!< The ID
	bool MissionDrop;               //!< Whether or not this loot table is a mission drop
	unsigned int sortPriority;           //!< The sorting priority
};

//! LootTable table
class CDLootTableTable : public CDTable {
private:
	std::vector<CDLootTable> entries;

public:

	//! Constructor
	CDLootTableTable(void);

	//! Destructor
	~CDLootTableTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDLootTable> Query(std::function<bool(CDLootTable)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	const std::vector<CDLootTable>& GetEntries(void) const;

};

