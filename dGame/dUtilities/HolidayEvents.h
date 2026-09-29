#ifndef HOLIDAYEVENTS_H
#define HOLIDAYEVENTS_H

#include <cstdint>
#include <string_view>

/**
 * Holiday events from the server-only EventGating table, as server scripts asked for them
 * (GetHolidayEvent{eventToCheck}.isValid in the live Lua).
 */
namespace HolidayEvents {
	/**
	 * Whether an event runs: its EventGating dates include now, or the server's event_1..event_8 setting names it (the
	 * live dates are all in 2010 and 2011, so this is how a server turns one on)
	 */
	bool IsActive(std::string_view eventName);

	// The same at a given Unix time, with the event_N settings passed in
	bool IsActive(std::string_view eventName, int64_t unixTime, const std::string_view* enabledEvents, size_t enabledCount);
}

#endif // HOLIDAYEVENTS_H
