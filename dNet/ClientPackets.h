/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include "RakNetTypes.h"

namespace ClientPackets {
	void HandleChatMessage(const SystemAddress& sysAddr, Packet* packet);
	void HandleClientPositionUpdate(const SystemAddress& sysAddr, Packet* packet);
	void HandleChatModerationRequest(const SystemAddress& sysAddr, Packet* packet);
};

#endif // CLIENTPACKETS_H
