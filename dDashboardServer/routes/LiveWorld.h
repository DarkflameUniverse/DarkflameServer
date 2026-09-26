#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

struct PlayerPositions;
struct HTTPContext;

/**
 * What is happening in the game right now, as seen by the dashboard: where players are (for the world map), plus
 * server-wide announcements and scheduled restarts.
 */
namespace LiveWorld {
	void RegisterRoutes();

	// A world server reported its players' positions
	void HandlePlayerPositions(const PlayerPositions& positions);

	// Main loop: push positions to watching browsers, send restart warnings, restart when it is time
	void Update();

	// Everyone online as the world servers last reported them: [{id, name, zone, instance, clone, x, y, z}]
	nlohmann::json OnlinePlayers();

	// Show an announcement in game, everywhere or only in the given zones; false when master isn't connected
	bool Announce(const std::string& title, const std::string& message, const std::vector<uint32_t>& zones = {});

	/**
	 * Schedule a restart in `minutes` (1 to 1440) with in-game warnings, audited as `actor`. One already scheduled is
	 * replaced when `replace`, or kept with an error. The error, if it wasn't scheduled
	 */
	std::optional<std::string> ScheduleRestart(int64_t minutes, const std::string& reason, const HTTPContext& actor, bool replace);

	// Scheduled restart for the status sent to every dashboard ({} when none)
	// The scheduled restart ({} when none); the staff member's name only with withStaffName
	nlohmann::json RestartStatus(bool withStaffName = false);
}
