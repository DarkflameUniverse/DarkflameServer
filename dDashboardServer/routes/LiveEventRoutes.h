#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "json.hpp"

struct HTTPContext;

/**
 * Live events staff start from the dashboard (Live Events page, live_events_manage): treasure hunts, bonus
 * multipliers, invasion waves and celebrations, for a set time in chosen zones. The event is stored (ILiveOps), announced
 * in game, and every world server of its zones runs it per instance (LiveEvents.h in dGame) and reports progress.
 */
namespace LiveEventRoutes {
	void RegisterRoutes();

	// Main loop: end events whose time is up (announcing how they went)
	void Update();

	// Running events for the public status page: [{title, type, zones, ends_at}]
	nlohmann::json PublicJson();

	// Tell every world to load the running live events and open challenges again
	void ReloadWorlds();

	// A live event part of a scheduled event (EventParts.h): {type, title, message, zones, instance, config}, checked as
	// when starting one on the Live Events page. The cleaned-up part, or nullopt and error
	std::optional<nlohmann::json> CheckPart(const nlohmann::json& body, std::string& error);

	// Start one now, running until endsAt (at most the longest a live event may run). Its id, or nullopt and error
	std::optional<uint64_t> StartPart(const nlohmann::json& body, int64_t endsAt, const HTTPContext& actor, std::string& error, std::string& message);

	// End it early; false when it had already ended
	bool EndPart(uint64_t id, const HTTPContext& actor, const std::string& reason);

	bool Running(uint64_t id);
}
