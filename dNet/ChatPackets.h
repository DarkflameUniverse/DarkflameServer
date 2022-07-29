/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CHATPACKETS_H
#define CHATPACKETS_H

struct SystemAddress;

#include <string>
#include "dCommonVars.h"

namespace ChatPackets {
	void SendChatMessage(const SystemAddress& sysAddr, char chatChannel, const std::string& senderName, LWOOBJID playerObjectID, bool senderMythran, const std::u16string& message);
	void SendSystemMessage(const SystemAddress& sysAddr, const std::u16string& message, bool broadcast = false);
	void SendMessageFail(const SystemAddress& sysAddr);
};

#endif // CHATPACKETS_H
