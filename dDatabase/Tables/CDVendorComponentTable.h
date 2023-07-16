#pragma once

// Custom Classes
#include "CDTable.h"

struct CDVendorComponent {
	unsigned int id;           //!< The Component ID
	float buyScalar;            //!< Buy Scalar (what does that mean?)
	float sellScalar;          //!< Sell Scalar (what does that mean?)
	float refreshTimeSeconds;  //!< The refresh time
	unsigned int LootMatrixIndex; //!< LootMatrixIndex of the vendor's items
};

namespace CDVendorComponentTable {
private:
	std::vector<CDVendorComponent> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDVendorComponent> Query(std::function<bool(CDVendorComponent)> predicate);

	std::vector<CDVendorComponent> GetEntries(void) const;
};

