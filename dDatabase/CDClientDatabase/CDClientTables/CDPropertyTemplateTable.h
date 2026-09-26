#pragma once
#include "CDTable.h"

struct CDPropertyTemplate {
	uint32_t id;
	uint32_t mapID;
	uint32_t vendorMapID;
	std::string spawnName;
};

class CDPropertyTemplateTable : public CDTable<CDPropertyTemplateTable, std::vector<CDPropertyTemplate>> {
public:
	void LoadValuesFromDatabase();

	CDPropertyTemplate GetByMapID(uint32_t mapID);

	// Every row (the news screen's top property slots are chosen from them)
	using CDTable::GetEntries;
};
