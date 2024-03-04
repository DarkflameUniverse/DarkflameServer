#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDBrickIDTableTable.hpp
 \brief Contains data for the BrickIDTable table
 */

 //! BrickIDTable Entry Struct
struct CDBrickIDTable {
	uint32_t NDObjectID;
	uint32_t LEGOBrickID;
};


//! BrickIDTable table
class CDBrickIDTableTable : public CDTable<CDBrickIDTableTable, std::vector<CDBrickIDTable>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDBrickIDTable> Query(std::function<bool(CDBrickIDTable)> predicate);
};
