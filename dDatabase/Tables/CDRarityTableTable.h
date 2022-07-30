#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDRarityTableTable.hpp
 \brief Contains data for the RarityTable table
 */

 //! RarityTable Entry Struct
struct CDRarityTable {
	unsigned int id;
	float randmax;
	unsigned int rarity;
	unsigned int RarityTableIndex;

	friend bool operator> (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity > c2.rarity;
	}

	friend bool operator>= (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity >= c2.rarity;
	}

	friend bool operator< (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity < c2.rarity;
	}

	friend bool operator<= (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity <= c2.rarity;
	}
};


//! RarityTable table
class CDRarityTableTable : public CDTable {
private:
	std::vector<CDRarityTable> entries;

public:

	//! Constructor
	CDRarityTableTable(void);

	//! Destructor
	~CDRarityTableTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDRarityTable> Query(std::function<bool(CDRarityTable)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	const std::vector<CDRarityTable>& GetEntries(void) const;

};

