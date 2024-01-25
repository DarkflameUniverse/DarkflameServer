#pragma once

// Custom Classes
#include "CDTable.h"

struct CDInventoryComponent {
	uint32_t id;                //!< The component ID for this object
	uint32_t itemid;            //!< The LOT of the object
	uint32_t count;             //!< The count of the items the object has
	bool equip;             //!< Whether or not to equip the item
};

class CDInventoryComponentTable : public CDTable<CDInventoryComponentTable> {
private:
	std::vector<CDInventoryComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDInventoryComponent> Query(std::function<bool(CDInventoryComponent)> predicate);

	const std::vector<CDInventoryComponent>& GetEntries() const;
};
