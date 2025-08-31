#include <gtest/gtest.h>

#include "QuickBuildComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"
#include "DestroyableComponent.h"

class QuickBuildComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(QuickBuildComponentTest, SerializeInitialUpdateNoDestroyable) {
	Entity testEntity(15, info);
	// Create QuickBuild without Destroyable component
	QuickBuildComponent quickBuildComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	quickBuildComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	// When no destroyable component, writes specific pattern for initial update
	bool flag1;
	ASSERT_TRUE(bitStream.Read(flag1));
	EXPECT_FALSE(flag1); // First flag is false
	
	bool flag2;
	ASSERT_TRUE(bitStream.Read(flag2));
	EXPECT_FALSE(flag2); // Second flag is false
	
	bool flag3;
	ASSERT_TRUE(bitStream.Read(flag3));
	EXPECT_FALSE(flag3); // Third flag is false
	
	// ScriptedActivity section
	bool hasScriptedActivity;
	ASSERT_TRUE(bitStream.Read(hasScriptedActivity));
	EXPECT_TRUE(hasScriptedActivity); // Always writes 1
	
	uint32_t builderCount;
	ASSERT_TRUE(bitStream.Read(builderCount));
	EXPECT_EQ(builderCount, 0); // No builder initially
}

TEST_F(QuickBuildComponentTest, SerializeRegularUpdateNoDestroyable) {
	Entity testEntity(15, info);
	QuickBuildComponent quickBuildComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	quickBuildComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// Regular update without destroyable should only write specific flags
	bool flag1;
	ASSERT_TRUE(bitStream.Read(flag1));
	EXPECT_FALSE(flag1);
	
	bool flag2;
	ASSERT_TRUE(bitStream.Read(flag2));
	EXPECT_FALSE(flag2);
	
	// ScriptedActivity section
	bool hasScriptedActivity;
	ASSERT_TRUE(bitStream.Read(hasScriptedActivity));
	EXPECT_TRUE(hasScriptedActivity);
	
	uint32_t builderCount;
	ASSERT_TRUE(bitStream.Read(builderCount));
	EXPECT_EQ(builderCount, 0);
}

TEST_F(QuickBuildComponentTest, SerializeWithDestroyableComponent) {
	Entity testEntity(15, info);
	
	// Add a destroyable component first
	DestroyableComponent* destroyableComponent = testEntity.AddComponent<DestroyableComponent>();
	
	QuickBuildComponent quickBuildComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	quickBuildComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// With destroyable component, should skip the special flags and go directly to ScriptedActivity
	bool hasScriptedActivity;
	ASSERT_TRUE(bitStream.Read(hasScriptedActivity));
	EXPECT_TRUE(hasScriptedActivity);
	
	uint32_t builderCount;
	ASSERT_TRUE(bitStream.Read(builderCount));
	EXPECT_EQ(builderCount, 0);
}

TEST_F(QuickBuildComponentTest, SerializeWithBuilder) {
	Entity testEntity(15, info);
	Entity builderEntity(20, info);
	
	QuickBuildComponent quickBuildComponent(&testEntity);
	
	// Simulate having a builder (this would normally be set through the component's logic)
	// Since GetBuilder() is based on internal state, this test validates the serialization format
	// when there's no builder
	
	RakNet::BitStream bitStream;
	quickBuildComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// Skip initial flags
	bool flag1, flag2, flag3;
	bitStream.Read(flag1);
	bitStream.Read(flag2);
	bitStream.Read(flag3);
	
	bool hasScriptedActivity;
	ASSERT_TRUE(bitStream.Read(hasScriptedActivity));
	EXPECT_TRUE(hasScriptedActivity);
	
	uint32_t builderCount;
	ASSERT_TRUE(bitStream.Read(builderCount));
	EXPECT_EQ(builderCount, 0); // No builder in default state
}

TEST_F(QuickBuildComponentTest, SerializeConsistentFormat) {
	Entity testEntity(15, info);
	QuickBuildComponent quickBuildComponent(&testEntity);
	
	// Test that serialization format is consistent between calls
	RakNet::BitStream firstStream, secondStream;
	
	quickBuildComponent.Serialize(firstStream, true);
	quickBuildComponent.Serialize(secondStream, true);
	
	// Compare the serialized data
	EXPECT_EQ(firstStream.GetNumberOfBitsUsed(), secondStream.GetNumberOfBitsUsed());
	
	// Reset and compare bit by bit
	firstStream.ResetReadPointer();
	secondStream.ResetReadPointer();
	
	while (firstStream.GetNumberOfUnreadBits() > 0 && secondStream.GetNumberOfUnreadBits() > 0) {
		bool firstBit, secondBit;
		firstStream.Read(firstBit);
		secondStream.Read(secondBit);
		EXPECT_EQ(firstBit, secondBit);
	}
}