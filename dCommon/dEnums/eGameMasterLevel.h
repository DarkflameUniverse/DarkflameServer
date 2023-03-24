#ifndef __EGAMEMASTERLEVEL__H__
#define __EGAMEMASTERLEVEL__H__

#include <cstdint>

enum class eGameMasterLevel : uint8_t {
	CIVILIAN = 0,             // Normal player.
	FORUM_MODERATOR = 1,      // No permissions on live servers.
	JUNIOR_MODERATOR = 2,     // Can kick/mute and pull chat logs.
	MODERATOR = 3,            // Can return lost items.
	SENIOR_MODERATOR = 4,     // Can ban.
	LEAD_MODERATOR = 5,       // Can approve properties.
	JUNIOR_DEVELOPER = 6,     // Junior developer & future content team. Civilan on live.
	INACTIVE_DEVELOPER = 7,   // Inactive developer, limited permissions.
	DEVELOPER = 8,            // Active developer, full permissions on live.
	OPERATOR = 9              // Can shutdown server for restarts & updates.
};


#endif  //!__EGAMEMASTERLEVEL__H__
