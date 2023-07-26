#pragma once

// Custom Classes
#include "CDTable.h"

struct CDVendorComponent {
	float buyScalar;            //!< Buy Scalar (what does that mean?)
	float sellScalar;          //!< Sell Scalar (what does that mean?)
	float refreshTimeSeconds;  //!< The refresh time
	unsigned int LootMatrixIndex; //!< LootMatrixIndex of the vendor's items
};

class CDVendorComponentTable : public CDTable<CDVendorComponentTable> {
private:
	std::unordered_map<uint32_t, CDVendorComponent> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	const std::optional<CDVendorComponent> Query(uint32_t id);
};

