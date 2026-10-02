#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "InventoryMessages.h"
#include "Item.h"
#include "eItemType.h"
#include "eReplicaComponentType.h"

#include <gtest/gtest.h>

// Moving within the inventories as LWOInventoryComponent_Common::msgMoveItemInInventory (0x00d20590) does.
class InventoryMoveTests : public GameDependenciesTest {
protected:
	static constexpr LOT HAT = 8000;
	static constexpr LOT MODEL = 8001;
	static constexpr LOT BRICK = 8002;

	std::unique_ptr<Entity> entity;
	InventoryComponent* inventory{};
	LWOOBJID nextId = 0x8000;

	void SetUp() override {
		SetUpDependencies();
		RegisterLot(HAT, eItemType::HAT);
		RegisterLot(MODEL, eItemType::MODEL);
		RegisterLot(BRICK, eItemType::BRICK);
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventory = entity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot, const eItemType type) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		component.itemType = static_cast<uint32_t>(type);
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	Item* Give(const LOT lot, const eInventoryType type, const uint32_t slot) {
		return new Item(nextId++, lot, inventory->GetInventory(type), slot, 1, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}

	void Move(const Item& item, const eInventoryType from, const int32_t to, const int32_t slot) {
		GameMessages::MoveItemInInventory msg;
		msg.iObjID = item.GetId();
		msg.inventoryType = from;
		msg.destInvType = to;
		msg.slot = slot;
		inventory->OnMoveItemInInventory(msg);
	}
};

TEST_F(InventoryMoveTests, TheInventoryALotMovesTo) {
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(HAT, eInventoryType::ITEMS), eInventoryType::ITEMS);
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(HAT, eInventoryType::VAULT_MODELS), eInventoryType::VAULT_ITEMS);
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(MODEL, eInventoryType::ITEMS), eInventoryType::MODELS);
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(MODEL, eInventoryType::VAULT_ITEMS), eInventoryType::VAULT_MODELS);
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(BRICK, eInventoryType::BRICKS), eInventoryType::BRICKS);
	EXPECT_EQ(InventoryComponent::GetInventoryTypeForMove(BRICK, eInventoryType::VAULT_ITEMS), eInventoryType::INVALID);
}

TEST_F(InventoryMoveTests, MovingWithinTheInventory) {
	auto* const hat = Give(HAT, eInventoryType::ITEMS, 0);
	auto* const other = Give(HAT, eInventoryType::ITEMS, 3);

	Move(*hat, eInventoryType::ITEMS, eInventoryType::INVALID, 3);
	EXPECT_EQ(hat->GetSlot(), 3u);
	EXPECT_EQ(other->GetSlot(), 0u);
}

TEST_F(InventoryMoveTests, OnlyToTheInventoryTheLotBelongsIn) {
	auto* const hat = Give(HAT, eInventoryType::ITEMS, 0);

	Move(*hat, eInventoryType::ITEMS, eInventoryType::MODELS, 5);
	Move(*hat, eInventoryType::ITEMS, eInventoryType::VAULT_MODELS, 5);
	EXPECT_EQ(hat->GetInventory()->GetType(), eInventoryType::ITEMS);
	EXPECT_EQ(hat->GetSlot(), 0u);

	// Looked up only in the inventory named
	inventory->GetInventory(eInventoryType::MODELS);
	Move(*hat, eInventoryType::MODELS, eInventoryType::ITEMS, 5);
	EXPECT_EQ(hat->GetSlot(), 0u);

	inventory->GetInventory(eInventoryType::VAULT_ITEMS);
	Move(*hat, eInventoryType::ITEMS, eInventoryType::VAULT_ITEMS, 5);
	EXPECT_EQ(hat->GetInventory()->GetType(), eInventoryType::VAULT_ITEMS);
	EXPECT_EQ(hat->GetSlot(), 5u);
}

TEST_F(InventoryMoveTests, TheSwappedItemGoesBackToTheSourceInventory) {
	auto* const hat = Give(HAT, eInventoryType::ITEMS, 2);
	auto* const vaulted = Give(HAT, eInventoryType::VAULT_ITEMS, 7);

	Move(*hat, eInventoryType::ITEMS, eInventoryType::VAULT_ITEMS, 7);
	EXPECT_EQ(hat->GetInventory()->GetType(), eInventoryType::VAULT_ITEMS);
	EXPECT_EQ(hat->GetSlot(), 7u);
	EXPECT_EQ(vaulted->GetInventory()->GetType(), eInventoryType::ITEMS);
	EXPECT_EQ(vaulted->GetSlot(), 2u);
}

TEST_F(InventoryMoveTests, NoSwapWithAnItemThatDoesNotBelongInTheSource) {
	auto* const hat = Give(HAT, eInventoryType::VAULT_ITEMS, 2);
	auto* const model = Give(MODEL, eInventoryType::VAULT_ITEMS, 7);
	inventory->GetInventory(eInventoryType::ITEMS);

	// The vault's model at slot 7 can't go to ITEMS
	auto* const itemsHat = Give(HAT, eInventoryType::ITEMS, 4);
	Move(*itemsHat, eInventoryType::ITEMS, eInventoryType::VAULT_ITEMS, 7);
	EXPECT_EQ(itemsHat->GetInventory()->GetType(), eInventoryType::ITEMS);
	EXPECT_EQ(model->GetInventory()->GetType(), eInventoryType::VAULT_ITEMS);
	EXPECT_EQ(hat->GetSlot(), 2u);
}
