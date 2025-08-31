#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "SwitchComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class SwitchComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	SwitchComponent* switchComponent;
	CBITSTREAM
	uint32_t flags = 0;
	
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		switchComponent = baseEntity->AddComponent<SwitchComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test SwitchComponent serialization with default inactive state
 */
TEST_F(SwitchComponentTest, SwitchComponentSerializeInactiveTest) {
	bitStream.Reset();
	
	// Test initial update with default inactive state
	switchComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool isActive;
	bitStream.Read(isActive);
	EXPECT_EQ(isActive, false); // Default state should be inactive
}

/**
 * Test SwitchComponent serialization with active state
 */
TEST_F(SwitchComponentTest, SwitchComponentSerializeActiveTest) {
	bitStream.Reset();
	
	// Set switch to active state
	switchComponent->SetActive(true);
	switchComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool isActive;
	bitStream.Read(isActive);
	EXPECT_EQ(isActive, true); // Should be active
}

/**
 * Test SwitchComponent serialization state changes
 */
TEST_F(SwitchComponentTest, SwitchComponentSerializeStateChangeTest) {
	bitStream.Reset();
	
	// Start inactive, then activate
	switchComponent->Serialize(bitStream, true);
	
	bool isActive1;
	bitStream.Read(isActive1);
	EXPECT_EQ(isActive1, false);
	
	// Reset and change to active
	bitStream.Reset();
	switchComponent->SetActive(true);
	switchComponent->Serialize(bitStream, false); // Regular update
	
	bool isActive2;
	bitStream.Read(isActive2);
	EXPECT_EQ(isActive2, true);
	
	// Reset and change back to inactive
	bitStream.Reset();
	switchComponent->SetActive(false);
	switchComponent->Serialize(bitStream, false); // Regular update
	
	bool isActive3;
	bitStream.Read(isActive3);
	EXPECT_EQ(isActive3, false);
}

/**
 * Test SwitchComponent serialization regular update behavior
 */
TEST_F(SwitchComponentTest, SwitchComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Set to active state
	switchComponent->SetActive(true);
	
	// Test regular update - should still serialize the boolean
	switchComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isActive;
	bitStream.Read(isActive);
	EXPECT_EQ(isActive, true);
	
	// SwitchComponent always serializes the active state regardless of update type
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 1); // Should have exactly 1 bit used
}