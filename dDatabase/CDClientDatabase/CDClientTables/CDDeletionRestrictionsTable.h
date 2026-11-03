#pragma once

#include "CDTable.h"

#include <optional>

enum class eDeletionRestrictionsCheckType : uint32_t;

struct CDDeletionRestriction {
	uint32_t id{};
	bool restricted{};
	// LOTs, zone ids or other rows of this table, depending on checkType; ids that aren't numbers are left out
	std::vector<uint32_t> ids;
	eDeletionRestrictionsCheckType checkType{};
};

class CDDeletionRestrictionsTable : public CDTable<CDDeletionRestrictionsTable, std::map<uint32_t, CDDeletionRestriction>> {
public:
	void LoadValuesFromDatabase();
	std::optional<CDDeletionRestriction> GetByID(uint32_t id) const;
};
