#ifndef __DASHBOARDNOTIFY__H__
#define __DASHBOARDNOTIFY__H__

#include <string>

#include "dCommonVars.h"

/**
 * Tells the web dashboard (through master) which rows this world server just wrote, so pages showing them update
 * right away. Changes are collected and sent in one message every half second; repeats in between are merged.
 */
namespace DashboardNotify {
	void Changed(const std::string& table, LWOOBJID id = 0);

	// Send what has been collected. Called from the world server loop; `force` sends immediately (shutdown).
	void Flush(bool force = false);

	// Report where this world's players are, every few seconds (called from the world server loop)
	void SendPlayerPositions(uint32_t instanceId);

	// Show a dashboard announcement to every player in this world
	void Announce(const std::string& title, const std::string& message);

	// The announcement's line in chat: "Title: message", without the colon after a title that already ends in
	// punctuation ("Challenge complete! Builders: ...")
	std::string ChatLine(const std::string& title, const std::string& message);
}

#endif  //!__DASHBOARDNOTIFY__H__
