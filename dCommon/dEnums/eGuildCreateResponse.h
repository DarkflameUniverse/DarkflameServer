#ifndef EGUILDCREATERESPONSE_H
#define EGUILDCREATERESPONSE_H

#include <cstdint>

// MSG_CLIENT_GUILD_CREATE_RESPONSE's result (docs/Guilds.md)
enum class eGuildCreateResponse : uint8_t {
	CREATED,  // "MSG_GUILD_GUILD_NAME_CREATED", then the client asks for the guild's data
	BAD_NAME, // "MSG_GUILD_GUILD_NAME_CANT_BE_USED"
	EXISTS,   // "MSG_GUILD_GUILD_NAME_ALREADY_IN_USE"
	FAILED,   // anything else: "MSG_GUILD_GUILD_COULD_NOT_BE_CREATED"
};

#endif // EGUILDCREATERESPONSE_H
