#pragma once

#include "../GameMessage.h"

class PlayEmote : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_PLAY_EMOTE; }

	int emoteID;
	LWOOBJID targetID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(emoteID);
		inStream->Read(targetID);
	};
};