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
	CDBrickIDTableTable();

	static const std::string GetTableName() { return "BrickIDTable"; };

	// Queries the table with a custom "where" clause
	std::vector<CDBrickIDTable> Query(std::function<bool(CDBrickIDTable)> predicate);

	std::vector<CDBrickIDTable> GetEntries(void) const;
};
