#pragma once

#include "CDTable.h"

#include <cstdint>
#include <map>

/**
 * CollectibleComponent: read only by the server (the 1.10.64 client has no string for the table).
 * requirement_mission is the mission a collectible belongs to, usually the achievement its collection task is in; for
 * some it is a mission that must be accepted first (e.g. 2040 for the Ninjago dragon relics, which the hidden
 * achievements 2064-2067 collect). -1 when the row has none.
 */
struct CDCollectibleComponent {
	int32_t id{};
	int32_t requirementMission{ -1 };
};

class CDCollectibleComponentTable : public CDTable<CDCollectibleComponentTable, std::map<int32_t, CDCollectibleComponent>> {
public:
	void LoadValuesFromDatabase();

	// The row for a component ID, or nothing
	[[nodiscard]] const CDCollectibleComponent* GetByID(int32_t id) const;
};
