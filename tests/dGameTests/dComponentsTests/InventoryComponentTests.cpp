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
 * Test that FindBehaviorSlot correctly maps equipLocation strings to BehaviorSlot enum values
 */
TEST_F(InventoryComponentTest, FindBehaviorSlotTest) {
	// Test the mappings from the issue comments
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("special_r"), BehaviorSlot::Primary);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("hair"), BehaviorSlot::Head);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("special_l"), BehaviorSlot::Offhand);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("clavicle"), BehaviorSlot::Neck);
	
	// Test that unknown equipLocations return Invalid
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("unknown"), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot(""), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("root"), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("leftHand"), BehaviorSlot::Invalid);
}