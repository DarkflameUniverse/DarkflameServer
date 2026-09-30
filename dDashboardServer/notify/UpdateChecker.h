#pragma once

#include "json_fwd.hpp"

/**
 * Checks GitHub for a newer release of the configured repository (update_check_repo) and, for development builds,
 * how many commits the build's branch has that the build doesn't. Nothing is ever downloaded or installed.
 *
 * Unauthenticated and gentle: at most every update_check_interval_hours (and one "Check now" a minute), with ETags so
 * unchanged answers don't count against GitHub's rate limit. Requests run on a worker thread; results come back in
 * Update() on the main loop, which logs them, tells master (UPDATE_STATUS, for its log) and pushes update_check to
 * open pages. Offline or rate limited, the last good answer stays and the error is shown next to it.
 */
namespace UpdateChecker {
	void Initialize();
	void Shutdown();

	// Main loop: start a check when one is due, take in finished ones. masterConnected: the link to master is up (the
	// last result is sent again each time it comes up, so a restarted master logs it too)
	void Update(bool masterConnected);

	// Check as soon as possible (no more than once a minute). Returns false when that was too soon or checks are off.
	bool CheckNow(std::string& error);

	// The running build and what the last check found, for the About page and /api/update_check
	nlohmann::json Json();

	void RegisterRoutes();
}
