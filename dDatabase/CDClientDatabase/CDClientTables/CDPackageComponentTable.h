#pragma once

// Custom Classes
#include "CDTable.h"

#include <cstdint>

struct CDPackageComponent {
	uint32_t id;
	uint32_t LootMatrixIndex;
	uint32_t packageType;
};

class CDPackageComponentTable : public CDTable<CDPackageComponentTable, std::vector<CDPackageComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDPackageComponent> Query(std::function<bool(CDPackageComponent)> predicate);
};
