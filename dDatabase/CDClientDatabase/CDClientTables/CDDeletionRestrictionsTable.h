#pragma once

#include "CDTable.h"

enum class eDeletionRestrictionsCheckType : uint32_t;

struct CDDeletionRestriction {
	uint32_t id;
	bool restricted;
	std::vector<uint32_t> ids;
	eDeletionRestrictionsCheckType checkType;
};

class CDDeletionRestrictionsTable : public CDTable<CDDeletionRestrictionsTable, std::map<uint32_t, CDDeletionRestriction>> {
public:
	void LoadValuesFromDatabase();
	const CDDeletionRestriction& GetByID(uint32_t id);

	static CDDeletionRestriction Default;
};
