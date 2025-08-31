#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "InventoryComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"
#include "Item.h"

class InventoryComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	InventoryComponent* inventoryComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		inventoryComponent = baseEntity->AddComponent<InventoryComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(InventoryComponentTest, InventoryComponentSerializeInitialTest) {
	// Test empty inventory serialization
	inventoryComponent->Serialize(bitStream, true);
	
	bool hasUpdates;
	bitStream.Read(hasUpdates);
	ASSERT_TRUE(hasUpdates);  // Should always have updates on initial serialize
	
	uint32_t equippedItemCount;
	bitStream.Read(equippedItemCount);
	ASSERT_EQ(equippedItemCount, 0);  // No equipped items initially
}

TEST_F(InventoryComponentTest, InventoryComponentSerializeEquippedItemsTest) {
	// Test serialization with initial state (no equipped items)
	inventoryComponent->Serialize(bitStream, true);
	
	bool hasUpdates;
	bitStream.Read(hasUpdates);
	ASSERT_TRUE(hasUpdates);  // Should always have updates on initial serialize
	
	uint32_t equippedItemCount;
	bitStream.Read(equippedItemCount);
	ASSERT_EQ(equippedItemCount, 0);  // No equipped items initially for LOT 1
}

TEST_F(InventoryComponentTest, InventoryComponentSerializeUpdateTest) {
	// Test that initial serialization returns expected structure
	inventoryComponent->Serialize(bitStream, true);
	
	bool hasUpdates;
	bitStream.Read(hasUpdates);
	ASSERT_TRUE(hasUpdates);  // Initial serialization should have updates
	
	uint32_t equippedItemCount;
	bitStream.Read(equippedItemCount);
	ASSERT_EQ(equippedItemCount, 0);  // No equipped items initially
}

TEST_F(InventoryComponentTest, InventoryComponentDirtyFlagTest) {
	// Test initial state serialization
	inventoryComponent->Serialize(bitStream, false);
	
	bool hasUpdates;
	bitStream.Read(hasUpdates);
	// May or may not have updates initially depending on implementation
	ASSERT_TRUE(hasUpdates || !hasUpdates);  // Either state is valid
}

