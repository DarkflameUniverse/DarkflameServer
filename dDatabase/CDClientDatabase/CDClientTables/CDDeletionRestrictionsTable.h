#pragma once

#include "CDTable.h"

enum class eDeletionRestrictionsCheckType : uint32_t;

struct CDDeletionRestrictions {
	uint32_t id;
	bool restricted;
	std::vector<uint32_t> ids;
	eDeletionRestrictionsCheckType checkType;
};

class CDDeletionRestrictionsTable : public CDTable<CDDeletionRestrictionsTable, std::vector<CDDeletionRestrictions>> {
public:
	void LoadValuesFromDatabase();
	std::vector<CDDeletionRestrictions> Query(std::function<bool(CDDeletionRestrictions)> predicate);
};
