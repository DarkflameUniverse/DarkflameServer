/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SLASHCOMMANDHANDLER_H
#define SLASHCOMMANDHANDLER_H

#include "RakNetTypes.h"
#include "eGameMasterLevel.h"
#include <string>

class Entity;

struct Command {
	std::string help;
	std::string info;
	std::vector<std::string> aliases;
	std::function<void(Entity*, const SystemAddress&,const std::string)> handle;
	eGameMasterLevel requiredLevel = eGameMasterLevel::OPERATOR;
};

namespace SlashCommandHandler {
	void HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr);
	void SendAnnouncement(const std::string& title, const std::string& message);
	void RegisterCommand(Command info);
	void Startup();
};

namespace GMZeroCommands {
	void Help(Entity* entity, const SystemAddress& sysAddr, const std::string args);
}

#endif // SLASHCOMMANDHANDLER_H
