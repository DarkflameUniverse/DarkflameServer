#pragma once

#include "../GameMessage.h"

class RequestSmashPlayer : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_SMASH_PLAYER; }

	void Deserialize(RakNet::BitStream* inStream) override {
		
	};
};