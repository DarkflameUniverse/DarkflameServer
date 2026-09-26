#ifndef __DASHBOARDACTIONS__H__
#define __DASHBOARDACTIONS__H__

#include <cstdint>
#include <functional>

#include "RakNetTypes.h"

struct PlayerActionRequest;

/**
 * Applies web dashboard moderation actions to players connected to this world server,
 * so bans, restrictions and rescues take effect immediately instead of on next login.
 */
namespace DashboardActions {
	// Apply the action to this world's players. Returns how many sessions or characters it applied to.
	uint32_t Apply(const PlayerActionRequest& request);

	/**
	 * Set how the world saves and removes a user. Kicks run it before closing the connection so the
	 * character is saved by the time the dashboard hears back, and nothing is saved after that.
	 */
	void SetLogoutHandler(std::function<void(const SystemAddress&)> handler);
}

#endif  //!__DASHBOARDACTIONS__H__
