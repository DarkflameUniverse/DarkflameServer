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
class CDBrickIDTableTable : public CDTable<CDBrickIDTableTable> {
private:
	std::vector<CDBrickIDTable> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDBrickIDTable> Query(std::function<bool(CDBrickIDTable)> predicate);

	const std::vector<CDBrickIDTable>& GetEntries() const;
};
