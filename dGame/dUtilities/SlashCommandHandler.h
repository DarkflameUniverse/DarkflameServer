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
	std::string description = "Default Command Description";
	std::string help = "Default Command Help Text";
	eGameMasterLevel requiredLevel = eGameMasterLevel::DEVELOPER;
	std::function<void(Entity*,const std::string)> handle;
};

namespace SlashCommandHandler {
	void Startup();
	void HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr);
	void SendAnnouncement(const std::string& title, const std::string& message);
	void RegisterCommand(Command info, std::string command);
};

namespace DEVGMCommands {
	void SetGMLevel(Entity* entity, const std::string args);
	void ToggleNameplate(Entity* entity, const std::string args);
	void ToggleSkipCinematics(Entity* entity, const std::string args);

}

namespace GMZeroCommands {
	void Help(Entity* entity, const std::string args);
}

namespace GreaterThanZeroCommands {
}

#endif // SLASHCOMMANDHANDLER_H
