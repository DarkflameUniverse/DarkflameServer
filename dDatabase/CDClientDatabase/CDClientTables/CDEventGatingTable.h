#pragma once

#include "CDTable.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * EventGating: read only by the server (the 1.10.64 client has no string for the table). A holiday event and the
 * Unix times (UTC) it ran between, inclusive: pirateDay 2011-09-19 00:00 to 2011-09-20 23:59:59, buildNexusTower,
 * frostburgh2010_notused and test rows. Server Lua asked for an event with GetHolidayEvent{eventToCheck = name}.isValid
 * (the Crux Prime random spawners switch to their pirateDay loads).
 */
struct CDEventGating {
	std::string eventName;
	int64_t dateStart{};
	int64_t dateEnd{};
};

class CDEventGatingTable : public CDTable<CDEventGatingTable, std::vector<CDEventGating>> {
public:
	void LoadValuesFromDatabase();

	// Whether the event has a row whose dates include the time
	[[nodiscard]] bool IsEventActive(std::string_view eventName, int64_t unixTime) const;
};
