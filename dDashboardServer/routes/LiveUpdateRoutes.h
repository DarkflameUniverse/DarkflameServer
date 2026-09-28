#pragma once

#include "json.hpp"

struct LiveUpdateStatus;

/**
 * The dashboard's "Live update" (docs/LiveUpdate.md): starts one on master and shows how it goes. The status comes
 * from master (LIVE_UPDATE_STATUS) and is pushed to browsers on the live_update socket topic.
 */
namespace LiveUpdateRoutes {
	void RegisterRoutes();

	// Master sent a status
	void HandleStatus(const LiveUpdateStatus& status);

	// Main loop: ask master for the status once connected (a restarted dashboard picks a running update up again)
	void Update();

	nlohmann::json StatusJson();
}
