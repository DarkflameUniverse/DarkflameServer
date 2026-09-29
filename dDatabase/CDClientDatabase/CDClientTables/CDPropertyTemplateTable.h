#pragma once
#include "CDTable.h"

struct CDPropertyTemplate {
	uint32_t id;
	uint32_t mapID;
	uint32_t vendorMapID;
	std::string spawnName;
	// Sent to the client in DownloadPropertyData; the defaults are what was sent
	// for every property before these columns were read
	float zoneX = 548.0f;
	float zoneY = 406.0f;
	float zoneZ = 178.0f;
	float maxBuildHeight = 128.0f;
};

class CDPropertyTemplateTable : public CDTable<CDPropertyTemplateTable, std::vector<CDPropertyTemplate>> {
public:
	void LoadValuesFromDatabase();

	CDPropertyTemplate GetByMapID(uint32_t mapID);

	// Every row (the news screen's top property slots are chosen from them)
	using CDTable::GetEntries;
};
