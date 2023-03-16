#pragma once

// Custom Classes
#include "CDTable.h"

struct CDPackageComponent {
	unsigned int id;
	unsigned int LootMatrixIndex;
	unsigned int packageType;
};

class CDPackageComponentTable : public CDTable {
private:
	std::vector<CDPackageComponent> entries;

public:
	CDPackageComponentTable(void);

	static const std::string GetTableName() { return "PackageComponent"; };

	// Queries the table with a custom "where" clause
	std::vector<CDPackageComponent> Query(std::function<bool(CDPackageComponent)> predicate);

	std::vector<CDPackageComponent> GetEntries(void) const;
};
