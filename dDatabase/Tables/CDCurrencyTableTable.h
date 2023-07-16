#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDCurrencyTableTable.hpp
 \brief Contains data for the CurrencyTable table
 */

 //! CurrencyTable Struct
struct CDCurrencyTable {
	unsigned int currencyIndex;         //!< The Currency Index
	unsigned int npcminlevel;           //!< The minimum level of the npc
	unsigned int minvalue;              //!< The minimum currency
	unsigned int maxvalue;              //!< The maximum currency
	unsigned int id;                    //!< The ID of the currency index
};

//! CurrencyTable table
namespace CDCurrencyTableTable {
private:
	std::vector<CDCurrencyTable> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDCurrencyTable> Query(std::function<bool(CDCurrencyTable)> predicate);

	std::vector<CDCurrencyTable> GetEntries(void) const;
};
