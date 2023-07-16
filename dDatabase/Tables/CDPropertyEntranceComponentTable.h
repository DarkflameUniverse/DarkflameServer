#pragma once
#include "CDTable.h"

struct CDPropertyEntranceComponent {
	uint32_t id;
	uint32_t mapID;
	std::string propertyName;
	bool isOnProperty;
	std::string groupType;
};

namespace CDPropertyEntranceComponentTable {
public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	CDPropertyEntranceComponent GetByID(uint32_t id);

	// Gets all the entries in the table
	[[nodiscard]] std::vector<CDPropertyEntranceComponent> GetEntries() const { return entries; }
private:
	std::vector<CDPropertyEntranceComponent> entries{};
	CDPropertyEntranceComponent defaultEntry{};
};
