#pragma once
#include "CDTable.h"

#include <cstdint>
#include <map>
#include <string>

// How a DeletionRestrictions row decides whether an item may be deleted (eDeletionRestrictionCheckType in the client)
enum class eDeletionRestrictionCheckType : int32_t {
	LOTS_INCLUDED = 0,     // allowed if another item of one of the LOTs is owned
	LOTS_EXCLUDED = 1,     // allowed if other items of every one of the LOTs are owned
	ANY_RESTRICTION = 2,   // allowed if any of the listed rows allows it
	ALL_RESTRICTIONS = 3,  // allowed if all of the listed rows allow it
	ZONE = 4,              // allowed in the listed map IDs
	ALWAYS_RESTRICTED = 5, // never allowed
};

struct CDDeletionRestriction {
	bool restricted{};
	std::string ids; // comma separated LOTs, row IDs or map IDs, depending on checkType
	int32_t checkType{};
};

class CDDeletionRestrictionsTable : public CDTable<CDDeletionRestrictionsTable, std::map<int32_t, CDDeletionRestriction>> {
public:
	void LoadValuesFromDatabase();
	// The row with this ID, or nullptr
	const CDDeletionRestriction* Get(int32_t id) const;
};
