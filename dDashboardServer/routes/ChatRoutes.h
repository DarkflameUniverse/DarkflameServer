#pragma once

#include <cstdint>

/**
 * The chat log (zone chat, whispers, team chat, and what the chat filter stopped), and an API for chat bridges:
 * read new messages (GET /api/chat?after=, or the chat_message WebSocket topic) and post into the game
 * (POST /api/chat/send, shown as "[label] name").
 */
void RegisterChatRoutes();

namespace ChatRoutes {
	// New messages since the last call went on the chat_message topic (called when the chat log's newest id moves)
	void PushNew(uint64_t newestId);
}
