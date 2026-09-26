#pragma once

#include <cstdint>
#include <vector>

#include "json.hpp"
#include "VanityEvents.h"

/**
 * Scheduled events (the Events page): parts that switch on and off together (a feature flag in event_1..event_8
 * through the Settings save path, vanity changes, a live event, announcements, a restart; EventParts.h), on once
 * between two times or by recurring rules (ScheduleRules.h), off or always on. The main loop starts each part when its
 * event turns on and ends it when it turns off, once each (what was done is kept with the part, so restarts in between
 * don't repeat or lose anything), audits it and raises alerts. Adding or changing a part needs the permission of the
 * page that does the same by hand; seeing the events needs any of them.
 */
namespace EventsCalendar {
	void RegisterRoutes();

	// Main loop: start and end the parts of events that turned on or off
	void Update();

	/**
	 * What the worlds would load: the vanity files with the vanity parts of the events on at {at} (unix, default now),
	 * or of the events {ids} instead (VanityEvents::LoadWorld, as the worlds call it)
	 */
	nlohmann::json VanityPreview(const nlohmann::json& body);

	// The vanity parts of the events that are on at `at`, in merge order (as the worlds load them)
	std::vector<VanityEvents::Changes> VanityChangesOn(int64_t at);

	/**
	 * The events with vanity parts, in merge order ({events: [{id, name, on, priority, modeName, nextStart, nextEnd}]}),
	 * which use each vanity file ({files: {name: [{id, name, on, use: overlay|on|off}]}}) and which take out each NPC
	 * ({removes: {name: [...]}})
	 */
	nlohmann::json VanityUses(int64_t now);
}
