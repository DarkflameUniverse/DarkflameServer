#pragma once

/**
 * Scheduled announcements: in-game messages repeated on a schedule (cron or @every, in UTC), everywhere or only in
 * chosen zones, between optional start and end dates. Sent the same way as the home page's announcement
 * (LiveWorld::Announce, through master).
 */
namespace Announcements {
	void RegisterRoutes();

	// Main loop: send what is due
	void Update();
}
