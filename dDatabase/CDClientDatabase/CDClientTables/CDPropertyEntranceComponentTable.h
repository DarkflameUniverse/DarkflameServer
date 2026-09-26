#pragma once
#include "CDTable.h"

struct CDPropertyEntranceComponent {
	uint32_t id;
	uint32_t mapID;
	std::string propertyName;
	bool isOnProperty;
	std::string groupType;
};

class CDPropertyEntranceComponentTable : public CDTable<CDPropertyEntranceComponentTable, std::vector<CDPropertyEntranceComponent>> {
public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	CDPropertyEntranceComponent GetByID(uint32_t id);

	// Every row (the news screen's top property slots are chosen from them)
	using CDTable::GetEntries;
};
