/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include "RakNetTypes.h"
#include "eGuildCreationResponse.h"
#include "dCommonVars.h"

namespace ClientPackets {
	void HandleChatMessage(const SystemAddress& sysAddr, Packet* packet);
	void HandleClientPositionUpdate(const SystemAddress& sysAddr, Packet* packet);
	void HandleChatModerationRequest(const SystemAddress& sysAddr, Packet* packet);

	// Guild stuff
	void HandleGuildCreation(const SystemAddress& sysAddr, Packet* packet);
	void SendGuildCreateResponse(const SystemAddress& sysAddr, eGuildCreationResponse guildResponse, LWOOBJID guildID, std::u16string& guildName);
};

#endif // CLIENTPACKETS_H
