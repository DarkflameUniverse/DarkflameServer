#pragma once

#include "../GameMessage.h"

class PickupItem : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_PICKUP_ITEM; }

	LWOOBJID lootObjectID;
	LWOOBJID playerID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(lootObjectID);
		inStream->Read(playerID);
	}
};