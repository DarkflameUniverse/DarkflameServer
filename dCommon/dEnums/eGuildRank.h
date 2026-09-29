#ifndef EGUILDRANK_H
#define EGUILDRANK_H

#include <cstdint>

// A guild member's rank, as the client names it in the guild list (0 and anything above 4: no name)
enum class eGuildRank : uint8_t {
	NONE,
	LEADER,
	OFFICER,
	VETERAN,
	RECRUIT,
};

// MSG_CLIENT_GUILD_REMOVE_PLAYER's reason
enum class eGuildLeaveReason : uint8_t {
	LEFT,
	KICKED,
};

#endif // EGUILDRANK_H
