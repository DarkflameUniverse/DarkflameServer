#pragma once

#include "../GameMessage.h"

class ParseChatMessage : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_PARSE_CHAT_MESSAGE; }

	int iClientState;
	std::u16string wsString;
	uint32_t wsStringLength;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(iClientState);
		inStream->Read(wsStringLength);
		for (uint32_t i = 0; i < wsStringLength; ++i) {
			uint16_t character;
			inStream->Read(character);
			wsString.push_back(character);
		}
	};
};