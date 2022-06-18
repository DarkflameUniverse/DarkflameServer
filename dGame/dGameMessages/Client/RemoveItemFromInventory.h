#pragma once

#include "../GameMessage.h"

class RemoveItemFromInventory : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REMOVE_ITEM_FROM_INVENTORY; }

	bool bConfirmed = false;
	bool bDeleteItem = true;
	bool bOutSuccess = false;
	bool eInvTypeIsDefault = false;
	int eInvType = INVENTORY_MAX;
	bool eLootTypeSourceIsDefault = false;
	int eLootTypeSource = LOOTTYPE_NONE;
	LWONameValue extraInfo;
	bool forceDeletion = true;
	bool iLootTypeSourceIsDefault = false;
	LWOOBJID iLootTypeSource = LWOOBJID_EMPTY;
	bool iObjIDIsDefault = false;
	LWOOBJID iObjID;
	bool iObjTemplateIsDefault = false;
	LOT iObjTemplate = LOT_NULL;
	bool iRequestingObjIDIsDefault = false;
	LWOOBJID iRequestingObjID = LWOOBJID_EMPTY;
	bool iStackCountIsDefault = false;
	uint32_t iStackCount = 1;
	bool iStackRemainingIsDefault = false;
	uint32_t iStackRemaining = 0;
	bool iSubkeyIsDefault = false;
	LWOOBJID iSubkey = LWOOBJID_EMPTY;
	bool iTradeIDIsDefault = false;
	LWOOBJID iTradeID = LWOOBJID_EMPTY;
	uint16_t nullTerm;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(bConfirmed);
		inStream->Read(bDeleteItem);
		inStream->Read(bOutSuccess);
		inStream->Read(eInvTypeIsDefault);
		if (eInvTypeIsDefault) inStream->Read(eInvType);
		inStream->Read(eLootTypeSourceIsDefault);
		if (eLootTypeSourceIsDefault) inStream->Read(eLootTypeSource);
		inStream->Read(extraInfo.length);
		if (extraInfo.length > 0) {
			for (uint32_t i = 0; i < extraInfo.length; ++i) {
				uint16_t character;
				inStream->Read(character);
				extraInfo.name.push_back(character);
			}
			inStream->Read(nullTerm);
		}
		inStream->Read(forceDeletion);
		inStream->Read(iLootTypeSourceIsDefault);
		if (iLootTypeSourceIsDefault) inStream->Read(iLootTypeSource);
		inStream->Read(iObjIDIsDefault);
		if (iObjIDIsDefault) inStream->Read(iObjID);
		inStream->Read(iObjTemplateIsDefault);
		if (iObjTemplateIsDefault) inStream->Read(iObjTemplate);
		inStream->Read(iRequestingObjIDIsDefault);
		if (iRequestingObjIDIsDefault) inStream->Read(iRequestingObjID);
		inStream->Read(iStackCountIsDefault);
		if (iStackCountIsDefault) inStream->Read(iStackCount);
		inStream->Read(iStackRemainingIsDefault);
		if (iStackRemainingIsDefault) inStream->Read(iStackRemaining);
		inStream->Read(iSubkeyIsDefault);
		if (iSubkeyIsDefault) inStream->Read(iSubkey);
		inStream->Read(iTradeIDIsDefault);
		if (iTradeIDIsDefault) inStream->Read(iTradeID);
	};
};