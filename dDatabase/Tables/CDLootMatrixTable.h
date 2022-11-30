#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDLootMatrixTable.hpp
 \brief Contains data for the ObjectSkills table
 */

 //! LootMatrix Struct
struct CDLootMatrix {
	unsigned int LootMatrixIndex;           //!< The Loot Matrix Index
	unsigned int LootTableIndex;            //!< The Loot Table Index
	unsigned int RarityTableIndex;          //!< The Rarity Table Index
	float percent;                   //!< The percent that this matrix is used?
	unsigned int minToDrop;                 //!< The minimum amount of loot from this matrix to drop
	unsigned int maxToDrop;                 //!< The maximum amount of loot from this matrix to drop
	unsigned int id;                        //!< The ID of the Loot Matrix
	unsigned int flagID;                    //!< ???
	UNUSED(std::string gate_version);          //!< The Gate Version
};

//! MissionNPCComponent table
class CDLootMatrixTable : public CDTable {
private:
	std::vector<CDLootMatrix> entries;

public:

	//! Constructor
	CDLootMatrixTable(void);

	//! Destructor
	~CDLootMatrixTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDLootMatrix> Query(std::function<bool(CDLootMatrix)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	const std::vector<CDLootMatrix>& GetEntries(void) const;

};

