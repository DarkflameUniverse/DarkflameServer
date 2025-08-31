#include <gtest/gtest.h>

#include "AchievementVendorComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"
#include "MessageType/Game.h"

class AchievementVendorComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(AchievementVendorComponentTest, Serialize) {
	Entity testEntity(15, info);
	// Test initial update
	AchievementVendorComponent achievementVendorComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	achievementVendorComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo); // Should always be true for initial update
	
	bool hasStandardCostItems;
	ASSERT_TRUE(bitStream.Read(hasStandardCostItems));
	EXPECT_TRUE(hasStandardCostItems); // Set by RefreshInventory
	
	bool hasMultiCostItems;
	ASSERT_TRUE(bitStream.Read(hasMultiCostItems));
	EXPECT_FALSE(hasMultiCostItems); // Default state
}

TEST_F(AchievementVendorComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	AchievementVendorComponent achievementVendorComponent(&testEntity);
	
	// Reset dirty flag by doing initial serialization
	RakNet::BitStream initStream;
	achievementVendorComponent.Serialize(initStream, true);
	
	// Test regular update with no changes
	RakNet::BitStream bitStream;
	achievementVendorComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_FALSE(hasVendorInfo); // No dirty flags, so no data
}

TEST_F(AchievementVendorComponentTest, SerializeWithDirtyVendor) {
	Entity testEntity(15, info);
	AchievementVendorComponent achievementVendorComponent(&testEntity);
	
	// Reset dirty flag
	RakNet::BitStream initStream;
	achievementVendorComponent.Serialize(initStream, true);
	
	// Make vendor dirty by changing state
	achievementVendorComponent.SetHasMultiCostItems(true);
	
	RakNet::BitStream bitStream;
	achievementVendorComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo); // Should be true due to dirty flag
	
	bool hasStandardCostItems;
	ASSERT_TRUE(bitStream.Read(hasStandardCostItems));
	EXPECT_TRUE(hasStandardCostItems);
	
	bool hasMultiCostItems;
	ASSERT_TRUE(bitStream.Read(hasMultiCostItems));
	EXPECT_TRUE(hasMultiCostItems); // Changed to true
}

TEST_F(AchievementVendorComponentTest, SerializeAfterDirtyCleared) {
	Entity testEntity(15, info);
	AchievementVendorComponent achievementVendorComponent(&testEntity);
	
	// Make dirty
	achievementVendorComponent.SetHasMultiCostItems(true);
	
	// Serialize once to clear dirty flag
	RakNet::BitStream firstStream;
	achievementVendorComponent.Serialize(firstStream, false);
	
	// Serialize again - should show no vendor info
	RakNet::BitStream secondStream;
	achievementVendorComponent.Serialize(secondStream, false);
	
	secondStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(secondStream.Read(hasVendorInfo));
	EXPECT_FALSE(hasVendorInfo); // Dirty flag should be cleared
}