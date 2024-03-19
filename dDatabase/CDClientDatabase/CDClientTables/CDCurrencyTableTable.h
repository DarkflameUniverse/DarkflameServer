#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDCurrencyTableTable.hpp
 \brief Contains data for the CurrencyTable table
 */

 //! CurrencyTable Struct
struct CDCurrencyTable {
	uint32_t currencyIndex;         //!< The Currency Index
	uint32_t npcminlevel;           //!< The minimum level of the npc
	uint32_t minvalue;              //!< The minimum currency
	uint32_t maxvalue;              //!< The maximum currency
	uint32_t id;                    //!< The ID of the currency index
};

//! CurrencyTable table
class CDCurrencyTableTable : public CDTable<CDCurrencyTableTable, std::vector<CDCurrencyTable>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDCurrencyTable> Query(std::function<bool(CDCurrencyTable)> predicate);
};
