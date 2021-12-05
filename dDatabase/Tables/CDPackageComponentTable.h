#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDPackageComponentTable.hpp
 \brief Contains data for the PackageComponent table
 */

 //! PackageComponent Entry Struct
struct CDPackageComponent {
	unsigned int id;
	unsigned int LootMatrixIndex;
	unsigned int packageType;
};


//! PackageComponent table
class CDPackageComponentTable : public CDTable {
private:
	std::vector<CDPackageComponent> entries;

public:

	//! Constructor
	CDPackageComponentTable(void);

	//! Destructor
	~CDPackageComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDPackageComponent> Query(std::function<bool(CDPackageComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDPackageComponent> GetEntries(void) const;

};
