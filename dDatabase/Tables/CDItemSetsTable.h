#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDItemSetsTable.hpp
 \brief Contains data for the ItemSets table
 */

 //! ZoneTable Struct
struct CDItemSets {
	unsigned int setID;             //!< The item set ID
	unsigned int locStatus;         //!< The loc status
	std::string itemIDs;       //!< THe item IDs
	unsigned int kitType;           //!< The item kit type
	unsigned int kitRank;           //!< The item kit rank
	unsigned int kitImage;          //!< The item kit image
	unsigned int skillSetWith2;     //!< The skill set with 2
	unsigned int skillSetWith3;     //!< The skill set with 3
	unsigned int skillSetWith4;     //!< The skill set with 4
	unsigned int skillSetWith5;     //!< The skill set with 5
	unsigned int skillSetWith6;     //!< The skill set with 6
	bool localize;          //!< Whether or localize
	std::string gate_version;  //!< The gate version
	unsigned int kitID;             //!< The kit ID
	float priority;         //!< The priority
};

//! ItemSets table
class CDItemSetsTable : public CDTable {
private:
	std::vector<CDItemSets> entries;

public:

	//! Constructor
	CDItemSetsTable(void);

	//! Destructor
	~CDItemSetsTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDItemSets> Query(std::function<bool(CDItemSets)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDItemSets> GetEntries(void) const;

};

