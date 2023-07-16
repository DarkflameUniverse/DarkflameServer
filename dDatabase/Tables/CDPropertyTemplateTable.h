#pragma once
#include "CDTable.h"

struct CDPropertyTemplate {
	uint32_t id;
	uint32_t mapID;
	uint32_t vendorMapID;
	std::string spawnName;
};

namespace CDPropertyTemplateTable {
public:
	void LoadTableIntoMemory();

	static const std::string GetTableName() { return "PropertyTemplate"; };
	CDPropertyTemplate GetByMapID(uint32_t mapID);
private:
	std::vector<CDPropertyTemplate> entries{};
	CDPropertyTemplate defaultEntry{};
};
