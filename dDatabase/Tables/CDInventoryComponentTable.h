#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDInventoryComponentTable.hpp
 \brief Contains data for the InventoryComponent table
 */

 //! ItemComponent Struct
struct CDInventoryComponent {
	unsigned int id;                //!< The component ID for this object
	unsigned int itemid;            //!< The LOT of the object
	unsigned int count;             //!< The count of the items the object has
	bool equip;             //!< Whether or not to equip the item
};

//! ItemComponent table
class CDInventoryComponentTable : public CDTable {
private:
	std::vector<CDInventoryComponent> entries;

public:

	//! Constructor
	CDInventoryComponentTable(void);

	//! Destructor
	~CDInventoryComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDInventoryComponent> Query(std::function<bool(CDInventoryComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDInventoryComponent> GetEntries(void) const;

};
