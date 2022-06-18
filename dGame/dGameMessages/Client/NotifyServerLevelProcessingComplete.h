#pragma once

#include "../GameMessage.h"

class NotifyServerLevelProcessingComplete : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_NOTIFY_SERVER_LEVEL_PROCESSING_COMPLETE; }

	void Deserialize(RakNet::BitStream* inStream) override {

	}
};