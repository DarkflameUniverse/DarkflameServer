#pragma once

#include "../GameMessage.h"

class MoveItemBetweenInventoryTypes : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_MOVE_ITEM_BETWEEN_INVENTORY_TYPES; }

	eInventoryType inventoryTypeA;
	eInventoryType inventoryTypeB;
	LWOOBJID objectID;
	bool showFlyingLoot = true;
	bool stackCountIsDefault = false;
	uint32_t stackCount = 1;
	bool templateIDIsDefault = false;
	LOT templateID = LOT_NULL;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(inventoryTypeA);
		inStream->Read(inventoryTypeB);
		inStream->Read(objectID);
		inStream->Read(showFlyingLoot);
		inStream->Read(stackCountIsDefault);
		if (stackCountIsDefault) inStream->Read(stackCount);
		inStream->Read(templateIDIsDefault);
		if (templateIDIsDefault) inStream->Read(templateID);
	};
};