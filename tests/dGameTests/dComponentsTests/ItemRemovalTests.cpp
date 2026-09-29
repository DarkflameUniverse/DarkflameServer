#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "eReplicaComponentType.h"

#include <gtest/gtest.h>

// An item taken out of its inventory is not freed on the spot: the inventory keeps it until its component's next
// update, so code still holding the item can finish with it.
class ItemRemovalTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	InventoryComponent* inventoryComponent{};
	Inventory* items{};
	LWOOBJID nextId = 0x7000;

	void SetUp() override {
		SetUpDependencies();
		RegisterLot(1000);
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
		entity = std::make_unique<Entity>(1, info);
		inventoryComponent = entity->AddComponent<InventoryComponent>(-1);
		items = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
	}

	static void RegisterLot(const LOT lot) {
		const auto componentID = static_cast<uint32_t>(96000 + lot);
		auto& registry = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registry.insert_or_assign(static_cast<uint64_t>(lot), componentID);
		registry.insert_or_assign(static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32 | static_cast<uint64_t>(lot), componentID);
		CDItemComponent component{};
		component.id = componentID;
		CDClientManager::GetEntriesMutable<CDItemComponentTable>().insert_or_assign(componentID, component);
	}

	Item* Give(const uint32_t count) {
		return new Item(nextId++, 1000, items, static_cast<uint32_t>(items->GetItems().size()), count, false, {}, LWOOBJID_EMPTY, LWOOBJID_EMPTY, eLootSourceType::NONE);
	}
};

TEST_F(ItemRemovalTests, ARemovedItemIsUsableUntilTheNextUpdate) {
	auto* const item = Give(1);
	const auto id = item->GetId();

	item->RemoveFromInventory();

	EXPECT_EQ(items->GetItems().count(id), 0u);
	EXPECT_TRUE(items->HasRetiredItems());
	// Still a valid object
	EXPECT_EQ(item->GetId(), id);
	EXPECT_EQ(item->GetCount(), 0u);

	inventoryComponent->Update(0.0f);
	EXPECT_FALSE(items->HasRetiredItems());
}

TEST_F(ItemRemovalTests, RemovingSeveralItemsInALoop) {
	Give(1);
	Give(2);
	Give(3);

	// Every item removed while the list is walked; each stays valid until the update
	auto held = items->GetItems();
	for (auto* const item : held | std::views::values) item->RemoveFromInventory();
	for (auto* const item : held | std::views::values) EXPECT_EQ(item->GetCount(), 0u);

	EXPECT_TRUE(items->GetItems().empty());
	inventoryComponent->Update(0.0f);
	EXPECT_FALSE(items->HasRetiredItems());
}

TEST_F(ItemRemovalTests, RetiredItemsAreFreedWithTheInventory) {
	Give(1)->RemoveFromInventory();
	EXPECT_TRUE(items->HasRetiredItems());
	// Freed by the inventory's destructor (checked by sanitizer builds)
	entity.reset();
}
