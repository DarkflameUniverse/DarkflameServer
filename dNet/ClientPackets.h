/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef CLIENTPACKETS_H
#define CLIENTPACKETS_H

#include <cstdint>
#include <string>

class PositionUpdate;

struct Packet;

struct ChatMessage {
	uint8_t chatChannel = 0;
	uint16_t unknown = 0;
	std::u16string message;
};

struct ChatModerationRequest {
	uint8_t chatLevel = 0;
	uint8_t requestID = 0;
	std::string receiver;
	std::string message;
};

namespace ClientPackets {
	ChatMessage HandleChatMessage(Packet* packet);
	PositionUpdate HandleClientPositionUpdate(Packet* packet);
	ChatModerationRequest HandleChatModerationRequest(Packet* packet);
	int32_t SendTop5HelpIssues(Packet* packet);
};

#endif // CLIENTPACKETS_H
