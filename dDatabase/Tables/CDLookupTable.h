#pragma once

// Custom Classes
#include "CDTable.h"

class CDLookupTable : public CDTable<CDLookupTable> {
private:
	std::unordered_map<std::string, int32_t> entries;
	std::unordered_multimap<int32_t, std::string> reverseEntries;

public:
	CDLookupTable();
	
	// Lookup
	int32_t Lookup(const std::string& key) const;

	// Reverse Lookup
	std::vector<std::string> ReverseLookup(int32_t value) const;
};

namespace rose {
	int32_t id(const std::string& key);
}
