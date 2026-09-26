#ifndef ERACINGCLIENTNOTIFICATIONTYPE_H
#define ERACINGCLIENTNOTIFICATIONTYPE_H

#include <cstdint>

// NotifyRacingClient event types, as in the client. Sent on the wire as an int32.
enum class eRacingClientNotificationType : int32_t {
	INVALID = 0,
	ACTIVITY_START,
	REWARD_PLAYER,
	EXIT,
	REPLAY,
	REMOVE_PLAYER,
	LEADERBOARD_UPDATED,
};

#endif // ERACINGCLIENTNOTIFICATIONTYPE_H
