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
class CDCurrencyTableTable : public CDTable {
private:
	std::vector<CDCurrencyTable> entries;

public:

	//! Constructor
	CDCurrencyTableTable(void);

	//! Destructor
	~CDCurrencyTableTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDCurrencyTable> Query(std::function<bool(CDCurrencyTable)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDCurrencyTable> GetEntries(void) const;

};
