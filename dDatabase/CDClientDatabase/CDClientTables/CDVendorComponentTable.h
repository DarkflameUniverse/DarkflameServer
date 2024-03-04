#pragma once

// Custom Classes
#include "CDTable.h"

struct CDVendorComponent {
	uint32_t id;           //!< The Component ID
	float buyScalar;            //!< Buy Scalar (what does that mean?)
	float sellScalar;          //!< Sell Scalar (what does that mean?)
	float refreshTimeSeconds;  //!< The refresh time
	uint32_t LootMatrixIndex; //!< LootMatrixIndex of the vendor's items
};

class CDVendorComponentTable : public CDTable<CDVendorComponentTable, std::vector<CDVendorComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	std::vector<CDVendorComponent> Query(std::function<bool(CDVendorComponent)> predicate);
};

