#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDVendorComponentTable.hpp
 \brief Contains data for the VendorComponent table
 */

 //! VendorComponent Struct
struct CDVendorComponent {
	unsigned int id;           //!< The Component ID
	float buyScalar;            //!< Buy Scalar (what does that mean?)
	float sellScalar;          //!< Sell Scalar (what does that mean?)
	float refreshTimeSeconds;  //!< The refresh time
	unsigned int LootMatrixIndex; //!< LootMatrixIndex of the vendor's items
};

//! VendorComponent table
class CDVendorComponentTable : public CDTable {
private:
	std::vector<CDVendorComponent> entries;

public:

	//! Constructor
	CDVendorComponentTable(void);

	//! Destructor
	~CDVendorComponentTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDVendorComponent> Query(std::function<bool(CDVendorComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDVendorComponent> GetEntries(void) const;

};

