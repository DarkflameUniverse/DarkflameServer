#ifndef CHATGUILDS_H
#define CHATGUILDS_H

class GuildManager;

// The chat server's GuildManager, connected to its players, database, chat filter and worlds (docs/Guilds.md)
namespace ChatGuilds {
	// Reads guild_max_members and guild_invite_timeout; call once after the database, config and chat filter are up
	void Initialize();
	GuildManager& Get();
}

#endif // CHATGUILDS_H
