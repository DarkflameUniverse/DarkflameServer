#pragma once

#include "../GameMessage.h"

class PlayerLoaded : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_PLAYER_LOADED; }

	void Deserialize(RakNet::BitStream* inStream) override {
		
	}
};