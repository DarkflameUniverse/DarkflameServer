#pragma once

#include "../GameMessage.h"

class EquipItem : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_EQUIP_ITEM; }

	bool immediate;
	LWOOBJID objectID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(immediate);
		inStream->Read(immediate); //twice?
		inStream->Read(objectID);
	};
};