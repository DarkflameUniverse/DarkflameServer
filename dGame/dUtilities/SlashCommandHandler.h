/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SLASHCOMMANDHANDLER_H
#define SLASHCOMMANDHANDLER_H

#include "RakNetTypes.h"
#include <string>

class Entity;

namespace SlashCommandHandler {
	void HandleChatCommand(const std::u16string& command, Entity* entity, const SystemAddress& sysAddr);
	bool CheckIfAccessibleZone(const unsigned int zoneID);

	void SendAnnouncement(const std::string& title, const std::string& message);
};

#endif // SLASHCOMMANDHANDLER_H
