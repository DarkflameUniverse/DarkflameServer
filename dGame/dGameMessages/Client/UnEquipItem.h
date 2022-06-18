#pragma once

#include "../GameMessage.h"

class UnEquipItem : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_UN_EQUIP_ITEM; }

	bool immediate;
	LWOOBJID objectID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(immediate);
		inStream->Read(immediate);
		inStream->Read(immediate);
		inStream->Read(objectID);
	};
};