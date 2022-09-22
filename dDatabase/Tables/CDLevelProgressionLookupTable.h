#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDLevelProgressionLookupTable.hpp
 \brief Contains data for the LevelProgressionLookup table
 */

 //! LevelProgressionLookup Entry Struct
struct CDLevelProgressionLookup {
	unsigned int id;                    //!< The Level ID
	unsigned int requiredUScore;        //!< The required LEGO Score
	std::string BehaviorEffect;    //!< The behavior effect attached to this
};

//! LevelProgressionLookup table
class CDLevelProgressionLookupTable : public CDTable {
private:
	std::vector<CDLevelProgressionLookup> entries;

public:

	//! Constructor
	CDLevelProgressionLookupTable(void);

	//! Destructor
	~CDLevelProgressionLookupTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDLevelProgressionLookup> Query(std::function<bool(CDLevelProgressionLookup)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDLevelProgressionLookup> GetEntries(void) const;

};
