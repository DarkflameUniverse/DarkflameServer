#pragma once

// Custom Classes
#include "CDTable.h"

struct CDInventoryComponent {
	unsigned int id;                //!< The component ID for this object
	unsigned int itemid;            //!< The LOT of the object
	unsigned int count;             //!< The count of the items the object has
	bool equip;             //!< Whether or not to equip the item
};

class CDInventoryComponentTable : public CDTable {
private:
	std::vector<CDInventoryComponent> entries;

public:
	CDInventoryComponentTable();

	static const std::string GetTableName() { return "InventoryComponent"; };

	// Queries the table with a custom "where" clause
	std::vector<CDInventoryComponent> Query(std::function<bool(CDInventoryComponent)> predicate);

	std::vector<CDInventoryComponent> GetEntries(void) const;
};
