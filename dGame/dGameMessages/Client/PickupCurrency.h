#pragma once

#include "../GameMessage.h"

class PickupCurrency : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_PICKUP_CURRENCY; }

	uint32_t currency;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(currency);
	}
};