#pragma once
#include "CDTable.h"

struct CDPropertyTemplate {
	uint32_t id;
	uint32_t mapID;
	uint32_t vendorMapID;
	std::string spawnName;
};

class CDPropertyTemplateTable : public CDTable {
public:
	CDPropertyTemplateTable();
	~CDPropertyTemplateTable();

	[[nodiscard]] std::string GetName() const override;
	CDPropertyTemplate GetByMapID(uint32_t mapID);
private:
	std::vector<CDPropertyTemplate> entries{};
	CDPropertyTemplate defaultEntry{};
};
