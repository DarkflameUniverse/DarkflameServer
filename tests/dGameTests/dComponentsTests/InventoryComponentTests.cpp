#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "InventoryComponent.h"
#include "BehaviorSlot.h"
#include "Entity.h"

class InventoryComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

/**
 * Test that FindBehaviorSlotByEquipLocation correctly maps equipLocation strings to BehaviorSlot enum values
 */
TEST_F(InventoryComponentTest, FindBehaviorSlotByEquipLocationTest) {
	// Test the mappings from the issue comments
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("special_r"), BehaviorSlot::Primary);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("hair"), BehaviorSlot::Head);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("special_l"), BehaviorSlot::Offhand);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("clavicle"), BehaviorSlot::Neck);
	
	// Test that unknown equipLocations return Invalid
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("unknown"), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation(""), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("root"), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlotByEquipLocation("leftHand"), BehaviorSlot::Invalid);
}