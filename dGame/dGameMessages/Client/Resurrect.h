#pragma once

#include "../GameMessage.h"

class Resurrect : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_RESURRECT; }

	bool bRezImmediately;

	void Deserialize(RakNet::BitStream* inStream) override {
		bRezImmediately = inStream->ReadBit();
	}
};