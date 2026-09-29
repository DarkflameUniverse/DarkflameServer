#include "InventoryMessages.h"

#include "BitStreamUtils.h"
#include "TeamManager.h"
#include "EntityManager.h"
#include "Game.h"
#include "Entity.h"
#include "GeneralUtils.h"
#include "Inventory.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "BrickByBrick.h"

#include <ranges>

namespace {
	// Name value text (the client's LwoNameValue on the wire): u32 length, the characters, then a null character
	// only if the text is not empty.
	void WriteNameValue(RakNet::BitStream& bitStream, const std::u16string& text) {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, text);
		if (!text.empty()) bitStream.Write<uint16_t>(0x00);
	}

	bool ReadNameValue(RakNet::BitStream& bitStream, std::u16string& text) {
		if (!BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, text)) return false;
		uint16_t nullTerminator{};
		return text.empty() || bitStream.Read(nullTerminator);
	}

	// Writes the flag and value of an optional field whose flag DLU always sets.
	template<typename T>
	void WriteAlways(RakNet::BitStream& bitStream, const T& value) {
		bitStream.Write1();
		bitStream.Write(value);
	}
}

namespace GameMessages {
	void AddItemToInventoryClientSync::SetItem(const Item& item) {
		bBound = item.GetBound();
		bIsBOE = item.GetInfo().isBOE;
		bIsBOP = item.GetInfo().isBOP;

		extraInfo.clear();
		for (const auto& data : item.GetConfig().values | std::views::values) {
			extraInfo += GeneralUtils::ASCIIToUTF16(data->GetString()) + u",";
		}
		if (extraInfo.length() > 0) extraInfo.pop_back(); // remove the last comma

		iObjTemplate = item.GetLot();
		invType = item.GetInventory()->GetType();
		itemsTotal = item.GetCount();
		slotID = item.GetSlot();
	}

	void AddItemToInventoryClientSync::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bBound);
		bitStream.Write(bIsBOE);
		bitStream.Write(bIsBOP);
		BitStreamUtils::WriteOptional(bitStream, eLootTypeSource, eLootSourceType::NONE);
		WriteNameValue(bitStream, extraInfo);
		bitStream.Write(iObjTemplate);
		BitStreamUtils::WriteOptional(bitStream, iSubkey, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, invType, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, itemCount, 1);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, itemsTotal, 0);
		bitStream.Write(newObjID);
		bitStream.Write(ni3FlyingLootPosit.x);
		bitStream.Write(ni3FlyingLootPosit.y);
		bitStream.Write(ni3FlyingLootPosit.z);
		bitStream.Write(showFlyingLoot);
		bitStream.Write(slotID);
	}

	bool AddItemToInventoryClientSync::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bBound));
		VALIDATE_READ(bitStream.Read(bIsBOE));
		VALIDATE_READ(bitStream.Read(bIsBOP));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, eLootTypeSource, eLootSourceType::NONE));
		VALIDATE_READ(ReadNameValue(bitStream, extraInfo));
		VALIDATE_READ(bitStream.Read(iObjTemplate));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iSubkey, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, invType, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, itemCount, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, itemsTotal, 0));
		VALIDATE_READ(bitStream.Read(newObjID));
		VALIDATE_READ(bitStream.Read(ni3FlyingLootPosit.x));
		VALIDATE_READ(bitStream.Read(ni3FlyingLootPosit.y));
		VALIDATE_READ(bitStream.Read(ni3FlyingLootPosit.z));
		VALIDATE_READ(bitStream.Read(showFlyingLoot));
		VALIDATE_READ(bitStream.Read(slotID));
		return true;
	}

	void SetInventorySize::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(inventoryType);
		bitStream.Write(size);
	}

	bool SetInventorySize::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(inventoryType));
		VALIDATE_READ(bitStream.Read(size));
		return true;
	}

	void RemoveItemFromInventory::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bConfirmed);
		bitStream.Write(bDeleteItem);
		bitStream.Write(bOutSuccess);
		WriteAlways(bitStream, eInvType);
		WriteAlways(bitStream, eLootTypeSource);
		WriteNameValue(bitStream, extraInfo);
		bitStream.Write(forceDeletion);
		BitStreamUtils::WriteOptional(bitStream, iLootTypeSource, LWOOBJID_EMPTY);
		WriteAlways(bitStream, iObjID);
		WriteAlways(bitStream, iObjTemplate);
		BitStreamUtils::WriteOptional(bitStream, iRequestingObjID, LWOOBJID_EMPTY);
		WriteAlways(bitStream, iStackCount);
		WriteAlways(bitStream, iStackRemaining);
		BitStreamUtils::WriteOptional(bitStream, iSubkey, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, iTradeID, LWOOBJID_EMPTY);
	}

	bool RemoveItemFromInventory::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bConfirmed));
		VALIDATE_READ(bitStream.Read(bDeleteItem));
		VALIDATE_READ(bitStream.Read(bOutSuccess));
		VALIDATE_READ(BitStreamUtils::ReadOptional<int32_t>(bitStream, eInvType, INVENTORY_MAX));
		VALIDATE_READ(BitStreamUtils::ReadOptional<int32_t>(bitStream, eLootTypeSource, LOOTTYPE_NONE));
		VALIDATE_READ(ReadNameValue(bitStream, extraInfo));
		VALIDATE_READ(bitStream.Read(forceDeletion));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iLootTypeSource, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iObjID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iObjTemplate, LOT_NULL));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iRequestingObjID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, iStackCount, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, iStackRemaining, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iSubkey, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iTradeID, LWOOBJID_EMPTY));
		return true;
	}

	void RemoveItemFromInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnRemoveItemFromInventory(*this);
	}

	void EquipInventory::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bIgnoreCooldown);
		bitStream.Write(bOutSuccess);
		bitStream.Write(itemToEquip);
	}

	bool EquipInventory::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bIgnoreCooldown));
		VALIDATE_READ(bitStream.Read(bOutSuccess));
		VALIDATE_READ(bitStream.Read(itemToEquip));
		return true;
	}

	void EquipInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnEquipInventory(*this);
	}

	void UnEquipInventory::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bEvenIfDead);
		bitStream.Write(bIgnoreCooldown);
		bitStream.Write(bOutSuccess);
		bitStream.Write(itemToUnequip);
		BitStreamUtils::WriteOptional(bitStream, replacementObjectID, LWOOBJID_EMPTY);
	}

	bool UnEquipInventory::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bEvenIfDead));
		VALIDATE_READ(bitStream.Read(bIgnoreCooldown));
		VALIDATE_READ(bitStream.Read(bOutSuccess));
		VALIDATE_READ(bitStream.Read(itemToUnequip));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, replacementObjectID, LWOOBJID_EMPTY));
		return true;
	}

	void UnEquipInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnUnEquipInventory(*this);
	}

	void MoveItemInInventory::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional<int32_t>(bitStream, destInvType, eInventoryType::INVALID);
		bitStream.Write(iObjID);
		bitStream.Write(inventoryType);
		bitStream.Write(responseCode);
		bitStream.Write(slot);
	}

	bool MoveItemInInventory::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional<int32_t>(bitStream, destInvType, eInventoryType::INVALID));
		VALIDATE_READ(bitStream.Read(iObjID));
		VALIDATE_READ(bitStream.Read(inventoryType));
		VALIDATE_READ(bitStream.Read(responseCode));
		VALIDATE_READ(bitStream.Read(slot));
		return true;
	}

	void MoveItemInInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnMoveItemInInventory(*this);
	}

	void MoveItemBetweenInventoryTypes::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(inventoryTypeA);
		bitStream.Write(inventoryTypeB);
		bitStream.Write(objectID);
		bitStream.Write(showFlyingLoot);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, stackCount, 1);
		BitStreamUtils::WriteOptional(bitStream, templateID, LOT_NULL);
	}

	bool MoveItemBetweenInventoryTypes::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(inventoryTypeA));
		VALIDATE_READ(bitStream.Read(inventoryTypeB));
		VALIDATE_READ(bitStream.Read(objectID));
		VALIDATE_READ(bitStream.Read(showFlyingLoot));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, stackCount, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, templateID, LOT_NULL));
		return true;
	}

	void MoveItemBetweenInventoryTypes::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnMoveItemBetweenInventoryTypes(*this);
	}

	void RequestMoveItemBetweenInventoryTypes::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAllowPartial);
		BitStreamUtils::WriteOptional(bitStream, destSlot, -1);
		BitStreamUtils::WriteOptional(bitStream, iStackCount, 1);
		BitStreamUtils::WriteOptional(bitStream, invTypeDst, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, invTypeSrc, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, itemID, LWOOBJID_EMPTY);
		bitStream.Write(showFlyingLoot);
		BitStreamUtils::WriteOptional(bitStream, subkey, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, itemLOT, LOT_NULL);
	}

	bool RequestMoveItemBetweenInventoryTypes::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAllowPartial));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, destSlot, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iStackCount, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, invTypeDst, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, invTypeSrc, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, itemID, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(showFlyingLoot));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, subkey, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, itemLOT, LOT_NULL));
		return true;
	}

	void RequestMoveItemBetweenInventoryTypes::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (invTypeDst != invTypeSrc && inventoryComponent) {
			inventoryComponent->OnRequestMoveItemBetweenInventoryTypes(*this, sysAddr);
			return;
		}

		ResponseMoveItemBetweenInventoryTypes response;
		response.target = entity.GetObjectID();
		response.inventoryTypeDestination = invTypeDst;
		response.inventoryTypeSource = invTypeSrc;
		response.response = eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC;
		response.SendToClient(sysAddr);
	}

	void ResponseMoveItemBetweenInventoryTypes::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, inventoryTypeDestination, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, inventoryTypeSource, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, response, eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC);
	}

	bool ResponseMoveItemBetweenInventoryTypes::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, inventoryTypeDestination, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, inventoryTypeSource, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, response, eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC));
		return true;
	}

	void MoveInventoryBatch::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bAllowPartial);
		bitStream.Write(bOutSuccess);
		BitStreamUtils::WriteOptional<uint32_t>(bitStream, count, 1);
		BitStreamUtils::WriteOptional(bitStream, dstBag, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, moveLOT, LOT_NULL);
		BitStreamUtils::WriteOptional(bitStream, moveSubkey, LWOOBJID_EMPTY);
		bitStream.Write(showFlyingLoot);
		BitStreamUtils::WriteOptional(bitStream, srcBag, eInventoryType::ITEMS);
		BitStreamUtils::WriteOptional(bitStream, startObjectID, LWOOBJID_EMPTY);
	}

	bool MoveInventoryBatch::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bAllowPartial));
		VALIDATE_READ(bitStream.Read(bOutSuccess));
		VALIDATE_READ(BitStreamUtils::ReadOptional<uint32_t>(bitStream, count, 1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, dstBag, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, moveLOT, LOT_NULL));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, moveSubkey, LWOOBJID_EMPTY));
		VALIDATE_READ(bitStream.Read(showFlyingLoot));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, srcBag, eInventoryType::ITEMS));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, startObjectID, LWOOBJID_EMPTY));
		return true;
	}

	void MoveInventoryBatch::Handle(Entity& entity, const SystemAddress& sysAddr) {
		BrickByBrick::MoveBricks(entity, *this);
	}

	void NotifyNotEnoughInvSpace::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(freeSlotsNeeded);
		BitStreamUtils::WriteOptional(bitStream, inventoryType, eInventoryType::ITEMS);
	}

	bool NotifyNotEnoughInvSpace::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(freeSlotsNeeded));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, inventoryType, eInventoryType::ITEMS));
		return true;
	}

	void MarkInventoryItemAsActive::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bActive);
		BitStreamUtils::WriteOptional(bitStream, iType, eUnequippableActiveType::INVALID);
		BitStreamUtils::WriteOptional(bitStream, itemID, LWOOBJID_EMPTY);
	}

	bool MarkInventoryItemAsActive::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bActive));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iType, eUnequippableActiveType::INVALID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, itemID, LWOOBJID_EMPTY));
		return true;
	}

	void ConsumeClientItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bSuccess);
		bitStream.Write(item);
	}

	bool ConsumeClientItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bSuccess));
		VALIDATE_READ(bitStream.Read(item));
		return true;
	}

	void ClientItemConsumed::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(item);
	}

	bool ClientItemConsumed::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(item));
		return true;
	}

	void ClientItemConsumed::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnClientItemConsumed(*this);
	}

	void UseNonEquipmentItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemToUse);
	}

	bool UseNonEquipmentItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemToUse));
		return true;
	}

	void UseNonEquipmentItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnUseNonEquipmentItem(*this);
	}

	void UseItemResult::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemTemplateID);
		bitStream.Write(useItemResult);
	}

	bool UseItemResult::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemTemplateID));
		VALIDATE_READ(bitStream.Read(useItemResult));
		return true;
	}

	void UseItemRequirementsResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(eUseResponse);
	}

	bool UseItemRequirementsResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(eUseResponse));
		return true;
	}

	void SetConsumableItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemTemplateID);
	}

	bool SetConsumableItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemTemplateID));
		return true;
	}

	void SetConsumableItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->SetConsumable(itemTemplateID);
	}

	void PushEquippedItemsState::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->PushEquippedItems();
	}

	void PopEquippedItemsState::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;
		inventoryComponent->PopEquippedItems();
		Game::entityManager->SerializeEntity(&entity); // so it updates on client side
	}

	void UpdateInventoryGroup::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, action);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, groupID);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, groupName);
		bitStream.Write(inventoryType);
		bitStream.Write(locked);
	}

	bool UpdateInventoryGroup::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, action));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, groupID));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, groupName, MAX_MESSAGE_LENGTH / 2));
		VALIDATE_READ(bitStream.Read(inventoryType));
		VALIDATE_READ(bitStream.Read(locked));
		return true;
	}

	void UpdateInventoryGroup::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnUpdateInventoryGroup(*this);
	}

	void UpdateInventoryGroupContents::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, action);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, groupID);
		bitStream.Write(inventoryType);
		bitStream.Write(lot);
	}

	bool UpdateInventoryGroupContents::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, action));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, groupID));
		VALIDATE_READ(bitStream.Read(inventoryType));
		VALIDATE_READ(bitStream.Read(lot));
		return true;
	}

	void UpdateInventoryGroupContents::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (inventoryComponent) inventoryComponent->OnUpdateInventoryGroupContents(*this);
	}

	void UseItemOnClient::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(itemLOT);
		bitStream.Write(itemToUse);
		bitStream.Write(itemType);
		bitStream.Write(playerId);
		bitStream.Write(targetPosition.x);
		bitStream.Write(targetPosition.y);
		bitStream.Write(targetPosition.z);
	}

	void DropClientLoot::Serialize(RakNet::BitStream& stream) const {
		stream.Write(bUsePosition);

		stream.Write(finalPosition != NiPoint3Constant::ZERO);
		if (finalPosition != NiPoint3Constant::ZERO) stream.Write(finalPosition);

		stream.Write(currency);
		stream.Write(item);
		stream.Write(lootID);
		stream.Write(ownerID);
		stream.Write(sourceID);

		stream.Write(spawnPos != NiPoint3Constant::ZERO);
		if (spawnPos != NiPoint3Constant::ZERO) stream.Write(spawnPos);
	}

	bool PickupItem::Deserialize(RakNet::BitStream& stream) {
		if (!stream.Read(lootID)) return false;
		if (!stream.Read(lootOwnerID)) return false;
		return true;
	}

	void PickupItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* team = TeamManager::Instance()->GetTeam(entity.GetObjectID());
		LOG("Has team %i picking up %llu:%llu", team != nullptr, lootID, lootOwnerID);
		if (team) {
			for (const auto memberId : team->members) {
				PickupItemEvent event(*this);
				event.Send(memberId);
				TeamPickupItem teamPickupMsg{};
				teamPickupMsg.target = lootID;
				teamPickupMsg.lootID = lootID;
				teamPickupMsg.lootOwnerID = lootOwnerID;
				const auto* const memberEntity = Game::entityManager->GetEntity(memberId);
				if (memberEntity) teamPickupMsg.Send(memberEntity->GetSystemAddress());
			}
		} else {
			entity.PickupItem(lootID);
		}
	}

	void TeamPickupItem::Serialize(RakNet::BitStream& stream) const {
		stream.Write(lootID);
		stream.Write(lootOwnerID);
	}

	bool UseItemOnClient::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(itemLOT));
		VALIDATE_READ(bitStream.Read(itemToUse));
		VALIDATE_READ(bitStream.Read(itemType));
		VALIDATE_READ(bitStream.Read(playerId));
		VALIDATE_READ(bitStream.Read(targetPosition.x));
		VALIDATE_READ(bitStream.Read(targetPosition.y));
		VALIDATE_READ(bitStream.Read(targetPosition.z));
		return true;
	}

	bool DropClientLoot::Deserialize(RakNet::BitStream& stream) {
		VALIDATE_READ(stream.Read(bUsePosition));
		VALIDATE_READ(BitStreamUtils::ReadOptional(stream, finalPosition, NiPoint3Constant::ZERO));
		VALIDATE_READ(stream.Read(currency));
		VALIDATE_READ(stream.Read(item));
		VALIDATE_READ(stream.Read(lootID));
		VALIDATE_READ(stream.Read(ownerID));
		VALIDATE_READ(stream.Read(sourceID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(stream, spawnPos, NiPoint3Constant::ZERO));
		return true;
	}

	void PickupItem::Serialize(RakNet::BitStream& stream) const {
		stream.Write(lootID);
		stream.Write(lootOwnerID);
	}

	bool TeamPickupItem::Deserialize(RakNet::BitStream& stream) {
		VALIDATE_READ(stream.Read(lootID));
		VALIDATE_READ(stream.Read(lootOwnerID));
		return true;
	}
}
