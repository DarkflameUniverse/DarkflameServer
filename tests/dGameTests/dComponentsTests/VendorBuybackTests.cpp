#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "VendorComponent.h"
#include "eReplicaComponentType.h"
#include "InventoryMessages.h"

#include <gtest/gtest.h>

class VendorBuybackTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};
	Inventory* buyback{};
	LWOOBJID nextId = 0x7000;

	void SetUp() override {
		SetUpDependencies();
		RegisterLot(3001, 1);  // one per slot
		RegisterLot(3002, 10); // stacks
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);
		buyback = inventory->GetInventory(eInventoryType::VENDOR_BUYBACK);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot, const int32_t stackSize) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.stackSize = stackSize;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	// The inventory owns the item
	Item* Give(Inventory* to, const LOT lot, const uint32_t count = 1) {
		return new Item(nextId++, lot, to, static_cast<uint32_t>(to->GetItems().size()), count, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}
};

TEST_F(VendorBuybackTests, KeepsTheSizeWhenFull) {
	for (size_t i = 0; i < VendorComponent::BUYBACK_SIZE; i++) Give(buyback, 3001);
	EXPECT_EQ(buyback->FindEmptySlot(), -1);
	EXPECT_EQ(buyback->GetSize(), VendorComponent::BUYBACK_SIZE);
}

TEST_F(VendorBuybackTests, SellingToAFullBuybackDropsTheOldest) {
	const auto oldest = nextId;
	for (size_t i = 0; i < VendorComponent::BUYBACK_SIZE; i++) Give(buyback, 3001);
	auto* const sold = Give(inventory->GetInventory(eInventoryType::ITEMS), 3001);

	VendorComponent::MakeRoomInBuyback(*inventory, *sold, 1);
	EXPECT_EQ(buyback->GetItems().size(), VendorComponent::BUYBACK_SIZE - 1);
	EXPECT_FALSE(buyback->GetItems().contains(oldest));
	EXPECT_TRUE(buyback->GetItems().contains(oldest + 1));
}

TEST_F(VendorBuybackTests, RoomLeftOnAStackKeepsEverything) {
	Give(buyback, 3002, 5);
	for (size_t i = 1; i < VendorComponent::BUYBACK_SIZE; i++) Give(buyback, 3001);
	auto* const sold = Give(inventory->GetInventory(eInventoryType::ITEMS), 3002, 5);

	VendorComponent::MakeRoomInBuyback(*inventory, *sold, 5);
	EXPECT_EQ(buyback->GetItems().size(), VendorComponent::BUYBACK_SIZE);
	VendorComponent::MakeRoomInBuyback(*inventory, *sold, 6);
	EXPECT_EQ(buyback->GetItems().size(), VendorComponent::BUYBACK_SIZE - 1);
}

TEST_F(VendorBuybackTests, DroppingTheOldestMovesTheRestUp) {
	const auto oldest = nextId;
	for (size_t i = 0; i < VendorComponent::BUYBACK_SIZE; i++) Give(buyback, 3001);
	auto* const sold = Give(inventory->GetInventory(eInventoryType::ITEMS), 3001);
	auto* const server = static_cast<dServerMock*>(Game::server);
	server->ClearSentPackets();

	VendorComponent::MakeRoomInBuyback(*inventory, *sold, 1);

	// The 26 left in slots 0 to 25 in their order, the sold item's slot free after them
	for (LWOOBJID id = oldest + 1; id < oldest + static_cast<LWOOBJID>(VendorComponent::BUYBACK_SIZE); id++) {
		EXPECT_EQ(buyback->FindItemById(id)->GetSlot(), static_cast<uint32_t>(id - oldest - 1));
	}
	EXPECT_EQ(buyback->FindEmptySlot(), static_cast<int32_t>(VendorComponent::BUYBACK_SIZE - 1));

	// A MoveItemInInventory for each, in the buyback inventory, as live sent them
	std::vector<std::pair<LWOOBJID, int32_t>> moves;
	for (const auto& packet : server->GetSentPackets()) {
		RakNet::BitStream bitStream(const_cast<uint8_t*>(packet.bytes.data()), packet.bytes.size(), false);
		LWOOBJID target{};
		MessageType::Game id{};
		if (!GameMessages::NetGameMsg::ReadPacketHeader(bitStream, target, id) || id != MessageType::Game::MOVE_ITEM_IN_INVENTORY) continue;
		GameMessages::MoveItemInInventory move;
		ASSERT_TRUE(move.Deserialize(bitStream));
		EXPECT_EQ(target, entity->GetObjectID());
		EXPECT_EQ(move.inventoryType, eInventoryType::VENDOR_BUYBACK);
		EXPECT_EQ(move.destInvType, eInventoryType::INVALID);
		EXPECT_EQ(move.responseCode, 0);
		moves.emplace_back(move.iObjID, move.slot);
	}
	ASSERT_EQ(moves.size(), VendorComponent::BUYBACK_SIZE - 1);
	for (size_t i = 0; i < moves.size(); i++) {
		EXPECT_EQ(moves[i].first, oldest + 1 + static_cast<LWOOBJID>(i));
		EXPECT_EQ(moves[i].second, static_cast<int32_t>(i));
	}
}

TEST_F(VendorBuybackTests, NothingMovesWhenNothingIsDropped) {
	Give(buyback, 3001);
	auto* const sold = Give(inventory->GetInventory(eInventoryType::ITEMS), 3001);
	auto* const server = static_cast<dServerMock*>(Game::server);
	server->ClearSentPackets();

	VendorComponent::MakeRoomInBuyback(*inventory, *sold, 1);
	EXPECT_TRUE(server->GetSentPackets().empty());
}
