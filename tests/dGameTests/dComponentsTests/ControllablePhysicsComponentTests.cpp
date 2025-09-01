#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class ControllablePhysicsComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	ControllablePhysicsComponent* controllablePhysicsComponent;
	CBITSTREAM
	uint32_t flags = 0;
	
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		controllablePhysicsComponent = baseEntity->AddComponent<ControllablePhysicsComponent>(1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test ControllablePhysicsComponent initial update serialization
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Test initial update serialization
	controllablePhysicsComponent->Serialize(bitStream, true);
	
	// ControllablePhysicsComponent inherits from PhysicsComponent, so it serializes
	// position data first, then its own data. Just verify it produces output.
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
	
	// Verify the component exists and has reasonable default values
	EXPECT_FALSE(controllablePhysicsComponent->GetIsOnRail());
	EXPECT_EQ(controllablePhysicsComponent->GetSpeedMultiplier(), 1.0f);
	EXPECT_EQ(controllablePhysicsComponent->GetGravityScale(), 1.0f);
}

/**
 * Test ControllablePhysicsComponent jetpack mode serialization
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeJetpackTest) {
	bitStream.Reset();
	
	// Set jetpack mode
	controllablePhysicsComponent->SetInJetpackMode(true);
	
	// Test serialization with jetpack mode
	controllablePhysicsComponent->Serialize(bitStream, true);
	
	// Verify it produces output and jetpack mode is set
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
	EXPECT_TRUE(controllablePhysicsComponent->GetInJetpackMode());
}

/**
 * Test ControllablePhysicsComponent regular update serialization
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Do an initial update to clear dirty flags
	controllablePhysicsComponent->Serialize(bitStream, true);
	
	bitStream.Reset();
	
	// Test regular update serialization
	controllablePhysicsComponent->Serialize(bitStream, false);
	
	// Should produce some output (at least physics position data)
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
}

/**
 * Test ControllablePhysicsComponent position change serialization
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializePositionChangeTest) {
	bitStream.Reset();
	
	// Change position to trigger dirty position flag
	NiPoint3 newPos(100.0f, 200.0f, 300.0f);
	controllablePhysicsComponent->SetPosition(newPos);
	
	// Test serialization with position change
	controllablePhysicsComponent->Serialize(bitStream, false);
	
	// Should produce output due to position change
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
	
	// Verify the position was set
	auto pos = controllablePhysicsComponent->GetPosition();
	EXPECT_EQ(pos.x, 100.0f);
	EXPECT_EQ(pos.y, 200.0f);
	EXPECT_EQ(pos.z, 300.0f);
}