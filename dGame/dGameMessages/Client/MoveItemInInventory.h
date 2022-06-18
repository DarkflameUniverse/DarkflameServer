#pragma once

#include "../GameMessage.h"

class MoveItemInInventory : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_MOVE_ITEM_IN_INVENTORY; }

	bool destInvTypeIsDefault = false;
	int32_t destInvType = eInventoryType::INVALID;
	LWOOBJID iObjID;
	int inventoryType;
	int responseCode;
	int slot;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(destInvTypeIsDefault);
		if (destInvTypeIsDefault) { inStream->Read(destInvType); }
		inStream->Read(iObjID);
		inStream->Read(inventoryType);
		inStream->Read(responseCode);
		inStream->Read(slot);
	};
};