#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDBrickIDTableTable.hpp
 \brief Contains data for the BrickIDTable table
 */

 //! BrickIDTable Entry Struct
struct CDBrickIDTable {
	unsigned int NDObjectID;
	unsigned int LEGOBrickID;
};


//! BrickIDTable table
class CDBrickIDTableTable : public CDTable {
private:
	std::vector<CDBrickIDTable> entries;

public:

	//! Constructor
	CDBrickIDTableTable(void);

	//! Destructor
	~CDBrickIDTableTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDBrickIDTable> Query(std::function<bool(CDBrickIDTable)> predicate);

	//! Gets all the entries in the table
	/*!
	   \return The entries
	 */
	std::vector<CDBrickIDTable> GetEntries(void) const;

};
