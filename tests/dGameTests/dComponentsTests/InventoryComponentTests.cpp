#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "InventoryComponent.h"
#include "BehaviorSlot.h"
#include "Entity.h"
#include "eItemType.h"

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
	// Test the mappings from the issue comments with appropriate item types
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("special_r", eItemType::RIGHT_HAND), BehaviorSlot::Primary);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("hair", eItemType::HAIR), BehaviorSlot::Head);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("special_l", eItemType::LEFT_HAND), BehaviorSlot::Offhand);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("clavicle", eItemType::NECK), BehaviorSlot::Neck);
	
	// Test consumable items always return Consumable slot regardless of equipLocation
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("special_r", eItemType::CONSUMABLE), BehaviorSlot::Consumable);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("hair", eItemType::CONSUMABLE), BehaviorSlot::Consumable);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("", eItemType::CONSUMABLE), BehaviorSlot::Consumable);
	
	// Test that unknown equipLocations return Invalid
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("unknown", eItemType::RIGHT_HAND), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("", eItemType::HAT), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("root", eItemType::LEGS), BehaviorSlot::Invalid);
	EXPECT_EQ(InventoryComponent::FindBehaviorSlot("leftHand", eItemType::LEFT_HAND), BehaviorSlot::Invalid);
}