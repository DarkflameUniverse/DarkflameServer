#ifndef INVENTORYMESSAGESLEGACY_H
#define INVENTORYMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages functions that InventoryMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions.
// SendMoveInventoryBatch is not here: nothing called it, and MoveInventoryBatch follows the client's layout.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "eReponseMoveItemBetweenInventoryTypeCode.h"
#include "eUnequippableActiveType.h"
#include "eUseItemResponse.h"
#include "Entity.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Inventory.h"
#include "Item.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ServiceType.h"

#include <ranges>
#include <string>

namespace LegacyGameMessages {
	inline void SendAddItemToInventoryClientSync(Entity* entity, const SystemAddress& sysAddr, Item* item, const LWOOBJID& objectID, bool showFlyingLoot, int itemCount, LWOOBJID subKey, eLootSourceType lootSourceType) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::ADD_ITEM_TO_INVENTORY_CLIENT_SYNC);
		bitStream.Write(item->GetBound());
		bitStream.Write(item->GetInfo().isBOE);
		bitStream.Write(item->GetInfo().isBOP);

		bitStream.Write(lootSourceType != eLootSourceType::NONE); // Loot source
		if (lootSourceType != eLootSourceType::NONE) bitStream.Write(lootSourceType);
		std::u16string extraInfo;

		const auto& config = item->GetConfig();

		for (const auto& data : config.values | std::views::values) {
			extraInfo += GeneralUtils::ASCIIToUTF16(data->GetString()) + u",";
		}

		if (extraInfo.length() > 0) extraInfo.pop_back(); // remove the last comma

		bitStream.Write<uint32_t>(extraInfo.size());
		if (extraInfo.size() > 0) {
			for (uint32_t i = 0; i < extraInfo.size(); ++i) {
				bitStream.Write<uint16_t>(extraInfo[i]);
			}
			bitStream.Write<uint16_t>(0x00);
		}

		bitStream.Write(item->GetLot());

		bitStream.Write(subKey != LWOOBJID_EMPTY);
		if (subKey != LWOOBJID_EMPTY) bitStream.Write(subKey);

		auto* inventory = item->GetInventory();
		const auto inventoryType = inventory->GetType();

		bitStream.Write(inventoryType != eInventoryType::ITEMS);
		if (inventoryType != eInventoryType::ITEMS) bitStream.Write(inventoryType);

		bitStream.Write(itemCount != 1);
		if (itemCount != 1) bitStream.Write(itemCount);

		const auto count = item->GetCount();

		bitStream.Write(count != 0); //items total
		if (count != 0) bitStream.Write(count);

		bitStream.Write(objectID);
		bitStream.Write(0.0f);
		bitStream.Write(0.0f);
		bitStream.Write(0.0f);
		bitStream.Write(showFlyingLoot);
		bitStream.Write(item->GetSlot());

		SEND_PACKET;
	}

	inline void SendSetInventorySize(Entity* entity, int invType, int size) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::SET_INVENTORY_SIZE);
		bitStream.Write(invType);
		bitStream.Write(size);

		SystemAddress sysAddr = entity->GetSystemAddress();
		SEND_PACKET;
	}

	inline void SendRemoveItemFromInventory(Entity* entity, const SystemAddress& sysAddr, LWOOBJID objectID, LOT templateID, int inventoryType, uint32_t stackCount, uint32_t stackRemaining) {
		CBITSTREAM;
		CMSGHEADER;
		// this is used for a lot more than just inventory trashing (trades, vendors, etc.) but for now since it's just used for that, that's all im going to implement
		bool bConfirmed = true;
		bool bDeleteItem = true;
		bool bOutSuccess = false;
		int eInvType = inventoryType;
		int eLootTypeSource = LOOTTYPE_NONE;
		bool forceDeletion = true;
		LWOOBJID iLootTypeSource = LWOOBJID_EMPTY;
		LWOOBJID iObjID = objectID;
		LOT iObjTemplate = templateID;
		LWOOBJID iRequestingObjID = LWOOBJID_EMPTY;
		uint32_t iStackCount = stackCount;
		uint32_t iStackRemaining = stackRemaining;
		LWOOBJID iSubkey = LWOOBJID_EMPTY;
		LWOOBJID iTradeID = LWOOBJID_EMPTY;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::REMOVE_ITEM_FROM_INVENTORY);
		bitStream.Write(bConfirmed);
		bitStream.Write(bDeleteItem);
		bitStream.Write(bOutSuccess);
		bitStream.Write1();
		bitStream.Write(eInvType);
		bitStream.Write1();
		bitStream.Write(eLootTypeSource);
		bitStream.Write<uint32_t>(0); //extra info
		//bitStream.Write<uint16_t>(0); //extra info
		bitStream.Write(forceDeletion);
		bitStream.Write0();
		bitStream.Write1();
		bitStream.Write(iObjID);
		bitStream.Write1();
		bitStream.Write(iObjTemplate);
		bitStream.Write0();
		bitStream.Write1();
		bitStream.Write(iStackCount);
		bitStream.Write1();
		bitStream.Write(iStackRemaining);
		bitStream.Write0();
		bitStream.Write0();

		SEND_PACKET;
	}

	inline void SendConsumeClientItem(Entity* entity, bool bSuccess, LWOOBJID item) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::CONSUME_CLIENT_ITEM);
		bitStream.Write(bSuccess);
		bitStream.Write(item);

		SystemAddress sysAddr = entity->GetSystemAddress();
		SEND_PACKET;
	}

	inline void SendUseItemResult(Entity* entity, LOT templateID, bool useItemResult) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::USE_ITEM_RESULT);
		bitStream.Write(templateID);
		bitStream.Write(useItemResult);

		SystemAddress sysAddr = entity->GetSystemAddress();
		SEND_PACKET;
	}

	inline void SendUseItemRequirementsResponse(LWOOBJID objectID, const SystemAddress& sysAddr, eUseItemResponse itemResponse) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectID);
		bitStream.Write(MessageType::Game::USE_ITEM_REQUIREMENTS_RESPONSE);

		bitStream.Write(itemResponse);

		SEND_PACKET;
	}

	inline void SendResponseMoveItemBetweenInventoryTypes(LWOOBJID objectId, const SystemAddress& sysAddr, eInventoryType inventoryTypeDestination, eInventoryType inventoryTypeSource, eReponseMoveItemBetweenInventoryTypeCode response) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::RESPONSE_MOVE_ITEM_BETWEEN_INVENTORY_TYPES);

		bitStream.Write(inventoryTypeDestination != eInventoryType::ITEMS);
		if (inventoryTypeDestination != eInventoryType::ITEMS) bitStream.Write(inventoryTypeDestination);

		bitStream.Write(inventoryTypeSource != eInventoryType::ITEMS);
		if (inventoryTypeSource != eInventoryType::ITEMS) bitStream.Write(inventoryTypeSource);

		bitStream.Write(response != eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC);
		if (response != eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC) bitStream.Write(response);

		SEND_PACKET;
	}

	inline void SendNotifyNotEnoughInvSpace(LWOOBJID objectId, uint32_t freeSlotsNeeded, eInventoryType inventoryType, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::VEHICLE_NOTIFY_FINISHED_RACE);

		bitStream.Write(freeSlotsNeeded);
		bitStream.Write(inventoryType != 0);
		if (inventoryType != 0) bitStream.Write(inventoryType);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendUpdateInventoryUi(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::UPDATE_INVENTORY_UI);

		SEND_PACKET;
	}


	inline void SendMarkInventoryItemAsActive(LWOOBJID objectId, bool bActive, eUnequippableActiveType iType, LWOOBJID itemID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::MARK_INVENTORY_ITEM_AS_ACTIVE);

		bitStream.Write(bActive);

		bitStream.Write(iType != eUnequippableActiveType::INVALID);
		if (iType != eUnequippableActiveType::INVALID) bitStream.Write(iType);

		bitStream.Write(itemID != LWOOBJID_EMPTY);
		if (itemID != LWOOBJID_EMPTY) bitStream.Write(itemID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}


	struct LegacyEquip { bool immediate{}; LWOOBJID objectID{}; };
	inline LegacyEquip ReadEquipItem(RakNet::BitStream& inStream) {
		bool immediate;
		LWOOBJID objectID;
		inStream.Read(immediate);
		inStream.Read(immediate); //twice?
		inStream.Read(objectID);
		return { immediate, objectID };
	}

	inline LegacyEquip ReadUnequipItem(RakNet::BitStream& inStream) {
		bool immediate;
		LWOOBJID objectID;
		inStream.Read(immediate);
		inStream.Read(immediate);
		inStream.Read(immediate);
		inStream.Read(objectID);
		return { immediate, objectID };
	}

	struct LegacyRemoveItem {
		bool bConfirmed; bool bDeleteItem; bool bOutSuccess; int eInvType; int eLootTypeSource; std::u16string extraInfo; bool forceDeletion;
		LWOOBJID iLootTypeSource; LWOOBJID iObjID; LOT iObjTemplate; LWOOBJID iRequestingObjID; uint32_t iStackCount; uint32_t iStackRemaining; LWOOBJID iSubkey; LWOOBJID iTradeID;
	};
	inline LegacyRemoveItem ReadRemoveItemFromInventory(RakNet::BitStream& inStream) {
		bool bConfirmed = false;
		bool bDeleteItem = true;
		bool bOutSuccess = false;
		bool eInvTypeIsDefault = false;
		int eInvType = INVENTORY_MAX;
		bool eLootTypeSourceIsDefault = false;
		int eLootTypeSource = LOOTTYPE_NONE;
		int32_t extraInfoLength = 0;
		std::u16string extraInfo;
		bool forceDeletion = true;
		bool iLootTypeSourceIsDefault = false;
		LWOOBJID iLootTypeSource = LWOOBJID_EMPTY;
		bool iObjIDIsDefault = false;
		LWOOBJID iObjID = LWOOBJID_EMPTY;
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

		inStream.Read(bConfirmed);
		inStream.Read(bDeleteItem);
		inStream.Read(bOutSuccess);
		inStream.Read(eInvTypeIsDefault);
		if (eInvTypeIsDefault) inStream.Read(eInvType);
		inStream.Read(eLootTypeSourceIsDefault);
		if (eLootTypeSourceIsDefault) inStream.Read(eLootTypeSource);
		inStream.Read(extraInfoLength);
		if (extraInfoLength > 0) {
			for (uint32_t i = 0; i < extraInfoLength; ++i) {
				uint16_t character;
				inStream.Read(character);
				extraInfo.push_back(character);
			}
			uint16_t nullTerm;
			inStream.Read(nullTerm);
		}
		inStream.Read(forceDeletion);
		inStream.Read(iLootTypeSourceIsDefault);
		if (iLootTypeSourceIsDefault) inStream.Read(iLootTypeSource);
		inStream.Read(iObjIDIsDefault);
		if (iObjIDIsDefault) inStream.Read(iObjID);
		inStream.Read(iObjTemplateIsDefault);
		if (iObjTemplateIsDefault) inStream.Read(iObjTemplate);
		inStream.Read(iRequestingObjIDIsDefault);
		if (iRequestingObjIDIsDefault) inStream.Read(iRequestingObjID);
		inStream.Read(iStackCountIsDefault);
		if (iStackCountIsDefault) inStream.Read(iStackCount);
		inStream.Read(iStackRemainingIsDefault);
		if (iStackRemainingIsDefault) inStream.Read(iStackRemaining);
		inStream.Read(iSubkeyIsDefault);
		if (iSubkeyIsDefault) inStream.Read(iSubkey);
		inStream.Read(iTradeIDIsDefault);
		if (iTradeIDIsDefault) inStream.Read(iTradeID);
		return { bConfirmed, bDeleteItem, bOutSuccess, eInvType, eLootTypeSource, extraInfo, forceDeletion, iLootTypeSource, iObjID, iObjTemplate, iRequestingObjID, iStackCount, iStackRemaining, iSubkey, iTradeID };
	}

	struct LegacyMoveItemInInventory { int32_t destInvType; LWOOBJID iObjID; int inventoryType; int responseCode; int slot; };
	inline LegacyMoveItemInInventory ReadMoveItemInInventory(RakNet::BitStream& inStream) {
		bool destInvTypeIsDefault = false;
		int32_t destInvType = eInventoryType::INVALID;
		LWOOBJID iObjID;
		int inventoryType;
		int responseCode;
		int slot;
		inStream.Read(destInvTypeIsDefault);
		if (destInvTypeIsDefault) { inStream.Read(destInvType); }
		inStream.Read(iObjID);
		inStream.Read(inventoryType);
		inStream.Read(responseCode);
		inStream.Read(slot);
		return { destInvType, iObjID, inventoryType, responseCode, slot };
	}

	struct LegacyMoveItemBetweenInventoryTypes { eInventoryType inventoryTypeA; eInventoryType inventoryTypeB; LWOOBJID objectID; bool showFlyingLoot; uint32_t stackCount; LOT templateID; };
	inline LegacyMoveItemBetweenInventoryTypes ReadMoveItemBetweenInventoryTypes(RakNet::BitStream& inStream) {
		eInventoryType inventoryTypeA;
		eInventoryType inventoryTypeB;
		LWOOBJID objectID;
		bool showFlyingLoot = true;
		bool stackCountIsDefault = false;
		uint32_t stackCount = 1;
		bool templateIDIsDefault = false;
		LOT templateID = LOT_NULL;

		inStream.Read(inventoryTypeA);
		inStream.Read(inventoryTypeB);
		inStream.Read(objectID);
		inStream.Read(showFlyingLoot);
		inStream.Read(stackCountIsDefault);
		if (stackCountIsDefault) inStream.Read(stackCount);
		inStream.Read(templateIDIsDefault);
		if (templateIDIsDefault) inStream.Read(templateID);
		return { inventoryTypeA, inventoryTypeB, objectID, showFlyingLoot, stackCount, templateID };
	}

	struct LegacyRequestMove { bool bAllowPartial; int32_t destSlot; int32_t iStackCount; eInventoryType invTypeDst; eInventoryType invTypeSrc; LWOOBJID itemID; bool showFlyingLoot; LWOOBJID subkey; LOT itemLOT; };
	inline LegacyRequestMove ReadRequestMoveItemBetweenInventoryTypes(RakNet::BitStream& inStream) {
		bool bAllowPartial{};
		int32_t destSlot = -1;
		int32_t iStackCount = 1;
		eInventoryType invTypeDst = ITEMS;
		eInventoryType invTypeSrc = ITEMS;
		LWOOBJID itemID = LWOOBJID_EMPTY;
		bool showFlyingLoot{};
		LWOOBJID subkey = LWOOBJID_EMPTY;
		LOT itemLOT = 0;

		bAllowPartial = inStream.ReadBit();
		if (inStream.ReadBit()) inStream.Read(destSlot);
		if (inStream.ReadBit()) inStream.Read(iStackCount);
		if (inStream.ReadBit()) inStream.Read(invTypeDst);
		if (inStream.ReadBit()) inStream.Read(invTypeSrc);
		if (inStream.ReadBit()) inStream.Read(itemID);
		showFlyingLoot = inStream.ReadBit();
		if (inStream.ReadBit()) inStream.Read(subkey);
		if (inStream.ReadBit()) inStream.Read(itemLOT);
		return { bAllowPartial, destSlot, iStackCount, invTypeDst, invTypeSrc, itemID, showFlyingLoot, subkey, itemLOT };
	}

	// HandleClientItemConsumed, HandleUseNonEquipmentItem and HandleSetConsumableItem each read one value.
	template<typename T>
	inline T ReadSingle(RakNet::BitStream& inStream) {
		T value;
		inStream.Read(value);
		return value;
	}

	// The two group handlers read into an InventoryComponent::GroupUpdate; its fields are locals here.
	struct LegacyGroupUpdate { bool ok; std::string action; std::string groupId; std::u16string groupName; eInventoryType inventory; bool locked; LOT lot; };
	inline LegacyGroupUpdate ReadUpdateInventoryGroup(RakNet::BitStream& inStream) {
		std::string action;
		std::u16string groupName;
		std::string groupId;
		eInventoryType inventory{};
		bool locked{}; // All groups are locked by default

		uint32_t size{};
		if (!inStream.Read(size)) return {};
		if (size > MAX_MESSAGE_LENGTH) return {}; // Bounds check before resize
		action.resize(size);
		if (!inStream.Read(action.data(), size)) return {};

		if (!inStream.Read(size)) return {};
		if (size > MAX_MESSAGE_LENGTH) return {}; // Bounds check before resize
		groupId.resize(size);
		if (!inStream.Read(groupId.data(), size)) return {};

		if (!inStream.Read(size)) return {};
		if (size > MAX_MESSAGE_LENGTH / 2) return {}; // Bounds check: size * 2 would overflow or exceed limit
		groupName.resize(size);
		if (!inStream.Read(reinterpret_cast<char*>(groupName.data()), size * 2)) return {};

		if (!inStream.Read(inventory)) return {};
		if (!inStream.Read(locked)) return {};
		return { true, action, groupId, groupName, inventory, locked, 0 };
	}

	inline LegacyGroupUpdate ReadUpdateInventoryGroupContents(RakNet::BitStream& inStream) {
		std::string action;
		std::string groupId;
		eInventoryType inventory{};
		LOT lot{};

		uint32_t size{};
		if (!inStream.Read(size)) return {};
		action.resize(size);
		if (!inStream.Read(action.data(), size)) return {};

		if (!inStream.Read(size)) return {};
		groupId.resize(size);
		if (!inStream.Read(groupId.data(), size)) return {};

		if (!inStream.Read(inventory)) return {};
		if (!inStream.Read(lot)) return {};
		return { true, action, groupId, u"", inventory, false, lot };
	}
}

#endif // INVENTORYMESSAGESLEGACY_H
