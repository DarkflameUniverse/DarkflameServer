#ifndef GUILDCOMMANDS_H
#define GUILDCOMMANDS_H

#include <string>

class Entity;
struct SystemAddress;

/**
 * Guild slash commands (docs/Guilds.md). The client has none: its guild chat tab sends "/g <text>", which reaches the
 * server like any unknown command, and it has no controls for kicking, ranks or disbanding. The chat server decides
 * everything; these only pass the request on.
 */
namespace GuildCommands {
	void Chat(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void OpenCreateBox(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Kick(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Rank(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Leader(Entity* entity, const SystemAddress& sysAddr, const std::string args);
	void Disband(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif // GUILDCOMMANDS_H
