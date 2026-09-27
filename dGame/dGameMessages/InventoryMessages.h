#ifndef INVENTORYMESSAGES_H
#define INVENTORYMESSAGES_H

#include "GameMessages.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"
#include "eReponseMoveItemBetweenInventoryTypeCode.h"
#include "eUnequippableActiveType.h"
#include "eUseItemResponse.h"
#include "NiPoint3.h"

#include <string>

class Item;

// Game messages for inventories and items: adding, removing, moving, equipping and using items, inventory sizes
// and groups.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// Received messages are handled by the target's InventoryComponent.
namespace GameMessages {
	// Server -> client.
	struct AddItemToInventoryClientSync : public NetGameMsg {
		AddItemToInventoryClientSync() : NetGameMsg(MessageType::Game::ADD_ITEM_TO_INVENTORY_CLIENT_SYNC) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// Fills the fields that describe the item itself (bound flags, extra info, LOT, inventory, stack total and
		// slot). The caller sets the rest.
		void SetItem(const Item& item);

		bool bBound{};
		bool bIsBOE{};
		bool bIsBOP{};
		eLootSourceType eLootTypeSource{ eLootSourceType::NONE }; // optional
		// Name value text: u32 length and the characters, then a null character if it is not empty.
		std::u16string extraInfo{};
		LOT iObjTemplate{};
		LWOOBJID iSubkey{ LWOOBJID_EMPTY }; // optional
		eInventoryType invType{ eInventoryType::ITEMS }; // optional
		int32_t itemCount{ 1 }; // optional
		uint32_t itemsTotal{ 0 }; // optional
		LWOOBJID newObjID{};
		NiPoint3 ni3FlyingLootPosit{};
		bool showFlyingLoot{};
		uint32_t slotID{};
	};

	// Server -> client.
	struct SetInventorySize : public NetGameMsg {
		SetInventorySize() : NetGameMsg(MessageType::Game::SET_INVENTORY_SIZE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eInventoryType inventoryType{};
		int32_t size{};
	};

	// Both directions: the client asks to delete (trash) an item, and the server tells the client an item left
	// its inventory. DLU always sends the flag of the fields marked "always sent" (they are still read as optional).
	struct RemoveItemFromInventory : public NetGameMsg {
		RemoveItemFromInventory() : NetGameMsg(MessageType::Game::REMOVE_ITEM_FROM_INVENTORY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bConfirmed{};
		bool bDeleteItem{ true };
		bool bOutSuccess{};
		int32_t eInvType{ INVENTORY_MAX }; // optional, always sent
		int32_t eLootTypeSource{ LOOTTYPE_NONE }; // optional, always sent
		// Name value text: u32 length and the characters, then a null character if it is not empty.
		std::u16string extraInfo{};
		bool forceDeletion{ true };
		LWOOBJID iLootTypeSource{ LWOOBJID_EMPTY }; // optional
		LWOOBJID iObjID{ LWOOBJID_EMPTY }; // optional, always sent
		LOT iObjTemplate{ LOT_NULL }; // optional, always sent
		LWOOBJID iRequestingObjID{ LWOOBJID_EMPTY }; // optional
		uint32_t iStackCount{ 1 }; // optional, always sent
		uint32_t iStackRemaining{ 0 }; // optional, always sent
		LWOOBJID iSubkey{ LWOOBJID_EMPTY }; // optional
		LWOOBJID iTradeID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server.
	struct EquipInventory : public NetGameMsg {
		EquipInventory() : NetGameMsg(MessageType::Game::EQUIP_INVENTORY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bIgnoreCooldown{};
		bool bOutSuccess{};
		LWOOBJID itemToEquip{};
	};

	// Client -> server. The client also sends an optional replacementObjectID after itemToUnequip, which DLU
	// has never read (and still does not).
	struct UnEquipInventory : public NetGameMsg {
		UnEquipInventory() : NetGameMsg(MessageType::Game::UN_EQUIP_INVENTORY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bEvenIfDead{};
		bool bIgnoreCooldown{};
		bool bOutSuccess{};
		LWOOBJID itemToUnequip{};
	};

	// Client -> server.
	struct MoveItemInInventory : public NetGameMsg {
		MoveItemInInventory() : NetGameMsg(MessageType::Game::MOVE_ITEM_IN_INVENTORY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t destInvType{ eInventoryType::INVALID }; // optional
		LWOOBJID iObjID{};
		int32_t inventoryType{};
		int32_t responseCode{};
		int32_t slot{};
	};

	// Client -> server.
	struct MoveItemBetweenInventoryTypes : public NetGameMsg {
		MoveItemBetweenInventoryTypes() : NetGameMsg(MessageType::Game::MOVE_ITEM_BETWEEN_INVENTORY_TYPES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		eInventoryType inventoryTypeA{};
		eInventoryType inventoryTypeB{};
		LWOOBJID objectID{};
		bool showFlyingLoot{ true };
		uint32_t stackCount{ 1 }; // optional
		LOT templateID{ LOT_NULL }; // optional
	};

	// Client -> server. Answered with ResponseMoveItemBetweenInventoryTypes.
	struct RequestMoveItemBetweenInventoryTypes : public NetGameMsg {
		RequestMoveItemBetweenInventoryTypes() : NetGameMsg(MessageType::Game::REQUEST_MOVE_ITEM_BETWEEN_INVENTORY_TYPES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bAllowPartial{};
		int32_t destSlot{ -1 }; // optional
		int32_t iStackCount{ 1 }; // optional
		eInventoryType invTypeDst{ eInventoryType::ITEMS }; // optional
		eInventoryType invTypeSrc{ eInventoryType::ITEMS }; // optional
		LWOOBJID itemID{ LWOOBJID_EMPTY }; // optional
		bool showFlyingLoot{};
		LWOOBJID subkey{ LWOOBJID_EMPTY }; // optional
		LOT itemLOT{ LOT_NULL }; // optional
	};

	// Server -> client.
	struct ResponseMoveItemBetweenInventoryTypes : public NetGameMsg {
		ResponseMoveItemBetweenInventoryTypes() : NetGameMsg(MessageType::Game::RESPONSE_MOVE_ITEM_BETWEEN_INVENTORY_TYPES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eInventoryType inventoryTypeDestination{ eInventoryType::ITEMS }; // optional
		eInventoryType inventoryTypeSource{ eInventoryType::ITEMS }; // optional
		eReponseMoveItemBetweenInventoryTypeCode response{ eReponseMoveItemBetweenInventoryTypeCode::FAIL_GENERIC }; // optional
	};

	// Server -> client. Moves count items out of srcBag (the client fills dstBag from a following
	// AddItemToInventoryClientSync). Laid out as the client reads it; DLU does not send it.
	struct MoveInventoryBatch : public NetGameMsg {
		MoveInventoryBatch() : NetGameMsg(MessageType::Game::MOVE_INVENTORY_BATCH) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bAllowPartial{};
		bool bOutSuccess{};
		uint32_t count{ 1 }; // optional
		eInventoryType dstBag{ eInventoryType::ITEMS }; // optional
		LOT moveLOT{ LOT_NULL }; // optional
		LWOOBJID moveSubkey{ LWOOBJID_EMPTY }; // optional
		bool showFlyingLoot{};
		eInventoryType srcBag{ eInventoryType::ITEMS }; // optional
		LWOOBJID startObjectID{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client. DLU sends this with the message ID of VehicleNotifyFinishedRace, not
	// NOTIFY_NOT_ENOUGH_INV_SPACE; kept as it has always been sent (see docs/PacketArchitecture.md: wire fixes
	// are separate changes).
	struct NotifyNotEnoughInvSpace : public NetGameMsg {
		NotifyNotEnoughInvSpace() : NetGameMsg(MessageType::Game::VEHICLE_NOTIFY_FINISHED_RACE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		uint32_t freeSlotsNeeded{};
		eInventoryType inventoryType{ eInventoryType::ITEMS }; // optional
	};

	// Server -> client. No payload. The client sends it to itself normally; DLU sends it to refresh the inventory
	// UI (its defaults get around items that would otherwise stay invisible).
	struct UpdateInventoryUi : public NetGameMsg {
		UpdateInventoryUi() : NetGameMsg(MessageType::Game::UPDATE_INVENTORY_UI) {}
	};

	// Server -> client. Marks a pet or mount item as in use (or no longer).
	struct MarkInventoryItemAsActive : public NetGameMsg {
		MarkInventoryItemAsActive() : NetGameMsg(MessageType::Game::MARK_INVENTORY_ITEM_AS_ACTIVE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bActive{};
		eUnequippableActiveType iType{ eUnequippableActiveType::INVALID }; // optional
		LWOOBJID itemID{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client.
	struct ConsumeClientItem : public NetGameMsg {
		ConsumeClientItem() : NetGameMsg(MessageType::Game::CONSUME_CLIENT_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bSuccess{};
		LWOOBJID item{};
	};

	// Client -> server.
	struct ClientItemConsumed : public NetGameMsg {
		ClientItemConsumed() : NetGameMsg(MessageType::Game::CLIENT_ITEM_CONSUMED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID item{};
	};

	// Client -> server.
	struct UseNonEquipmentItem : public NetGameMsg {
		UseNonEquipmentItem() : NetGameMsg(MessageType::Game::USE_NON_EQUIPMENT_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID itemToUse{};
	};

	// Server -> client.
	struct UseItemResult : public NetGameMsg {
		UseItemResult() : NetGameMsg(MessageType::Game::USE_ITEM_RESULT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LOT itemTemplateID{};
		bool useItemResult{};
	};

	// Server -> client.
	struct UseItemRequirementsResponse : public NetGameMsg {
		UseItemRequirementsResponse() : NetGameMsg(MessageType::Game::USE_ITEM_REQUIREMENTS_RESPONSE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eUseItemResponse eUseResponse{};
	};

	// Client -> server.
	struct SetConsumableItem : public NetGameMsg {
		SetConsumableItem() : NetGameMsg(MessageType::Game::SET_CONSUMABLE_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LOT itemTemplateID{};
	};

	// Client -> server. No payload.
	struct PushEquippedItemsState : public NetGameMsg {
		PushEquippedItemsState() : NetGameMsg(MessageType::Game::PUSH_EQUIPPED_ITEMS_STATE) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server. No payload.
	struct PopEquippedItemsState : public NetGameMsg {
		PopEquippedItemsState() : NetGameMsg(MessageType::Game::POP_EQUIPPED_ITEMS_STATE) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server. Adds, modifies or removes an inventory group.
	struct UpdateInventoryGroup : public NetGameMsg {
		UpdateInventoryGroup() : NetGameMsg(MessageType::Game::UPDATE_INVENTORY_GROUP) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::string action{}; // ADD, MODIFY or REMOVE
		std::string groupID{};
		std::u16string groupName{};
		eInventoryType inventoryType{};
		bool locked{};
	};

	// Client -> server. Adds a LOT to or removes one from an inventory group.
	struct UpdateInventoryGroupContents : public NetGameMsg {
		UpdateInventoryGroupContents() : NetGameMsg(MessageType::Game::UPDATE_INVENTORY_GROUP_CONTENTS) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::string action{}; // ADD or REMOVE
		std::string groupID{};
		eInventoryType inventoryType{};
		LOT lot{};
	};
};

#endif // INVENTORYMESSAGES_H
