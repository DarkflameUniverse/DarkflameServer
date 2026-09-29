#ifndef EGUILDINVITERESPONSE_H
#define EGUILDINVITERESPONSE_H

#include <cstdint>

// MSG_CLIENT_GUILD_INVITE_INITIAL_RESPONSE's code: what happened to the invite the player sent (docs/Guilds.md)
enum class eGuildInviteResponse : uint8_t {
	SENT,             // "MSG_GUILD_GUILD_INVITE_SENT_TO_NAME"
	NOT_ONLINE,       // "MSG_GENERIC_NAME_IS_NOT_ONLINE"
	ALREADY_IN_GUILD, // "MSG_GUILD_NAME_IS_ALREADY_IN_A_GUILD"
	INVITE_PENDING,   // "MSG_GUILD_NAME_ALREADY_HAS_A_GUILD_INVITE_PENDING"
	FAILED,           // anything else: "MSG_GUILD_COULD_NOT_INVITE_NAME"
};

// MSG_CLIENT_GUILD_INVITE_FINAL_RESPONSE's code: the invited player's answer, told to the inviter
enum class eGuildInviteFinalResponse : uint8_t {
	JOINED,     // "MSG_GUILD_NAME_HAS_JOINED_THE_GUILD"
	DECLINED,   // "MSG_GUILD_NAME_DECLINED_YOUR_INVITATION"
	NOT_ONLINE, // "MSG_GENERIC_NAME_IS_NOT_ONLINE"
	FAILED,     // anything else: "CLIENTMSG_COULD_NOT_INVITE_NAME"
};

#endif // EGUILDINVITERESPONSE_H
