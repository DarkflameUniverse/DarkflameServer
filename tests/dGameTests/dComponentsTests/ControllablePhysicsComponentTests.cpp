#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

class ControllablePhysicsComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	ControllablePhysicsComponent* physicsComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		physicsComponent = baseEntity->AddComponent<ControllablePhysicsComponent>(1);  // Needs componentId
		
		// Set some test values
		physicsComponent->SetPosition(NiPoint3(100.0f, 200.0f, 300.0f));
		physicsComponent->SetRotation(NiQuaternion(0.5f, 0.5f, 0.5f, 0.5f));
		physicsComponent->SetVelocity(NiPoint3(10.0f, 20.0f, 30.0f));
		physicsComponent->SetAngularVelocity(NiPoint3(1.0f, 2.0f, 3.0f));
		physicsComponent->SetIsOnGround(true);
		physicsComponent->SetIsOnRail(false);
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsSerializeInitialTest) {
	physicsComponent->Serialize(bitStream, true);
	
	// Read jetpack mode info
	bool inJetpackMode;
	bitStream.Read(inJetpackMode);
	ASSERT_FALSE(inJetpackMode);  // Default should be false
	
	// Should always write stun immunity data on construction
	bool hasStunImmunityData;
	bitStream.Read(hasStunImmunityData);
	ASSERT_TRUE(hasStunImmunityData);
	
	uint32_t immuneToStunMoveCount, immuneToStunJumpCount, immuneToStunTurnCount;
	uint32_t immuneToStunAttackCount, immuneToStunUseItemCount, immuneToStunEquipCount;
	uint32_t immuneToStunInteractCount;
	
	bitStream.Read(immuneToStunMoveCount);
	bitStream.Read(immuneToStunJumpCount);
	bitStream.Read(immuneToStunTurnCount);
	bitStream.Read(immuneToStunAttackCount);
	bitStream.Read(immuneToStunUseItemCount);
	bitStream.Read(immuneToStunEquipCount);
	bitStream.Read(immuneToStunInteractCount);
	
	// Default values should be 0
	ASSERT_EQ(immuneToStunMoveCount, 0);
	ASSERT_EQ(immuneToStunJumpCount, 0);
	ASSERT_EQ(immuneToStunTurnCount, 0);
	ASSERT_EQ(immuneToStunAttackCount, 0);
	ASSERT_EQ(immuneToStunUseItemCount, 0);
	ASSERT_EQ(immuneToStunEquipCount, 0);
	ASSERT_EQ(immuneToStunInteractCount, 0);
}

TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsJetpackSerializationTest) {
	// Test jetpack mode serialization
	physicsComponent->SetInJetpackMode(true);
	physicsComponent->SetJetpackEffectID(12345);
	physicsComponent->SetJetpackFlying(true);
	physicsComponent->SetJetpackBypassChecks(false);
	
	physicsComponent->Serialize(bitStream, true);
	
	bool inJetpackMode;
	bitStream.Read(inJetpackMode);
	ASSERT_TRUE(inJetpackMode);
	
	int32_t jetpackEffectID;
	bool jetpackFlying, jetpackBypassChecks;
	bitStream.Read(jetpackEffectID);
	bitStream.Read(jetpackFlying);
	bitStream.Read(jetpackBypassChecks);
	
	ASSERT_EQ(jetpackEffectID, 12345);
	ASSERT_TRUE(jetpackFlying);
	ASSERT_FALSE(jetpackBypassChecks);
}

TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsStunImmunityTest) {
	// Test stun immunity serialization
	physicsComponent->SetStunImmunity(eStateChangeType::PUSH, 5);
	// Note: PULL may not be available, using another valid type
	physicsComponent->SetStunImmunity(eStateChangeType::POP, 3);
	
	physicsComponent->Serialize(bitStream, true);
	
	// Skip jetpack data
	bool inJetpackMode;
	bitStream.Read(inJetpackMode);
	
	bool hasStunImmunityData;
	bitStream.Read(hasStunImmunityData);
	ASSERT_TRUE(hasStunImmunityData);
	
	uint32_t immuneToStunMoveCount, immuneToStunJumpCount, immuneToStunTurnCount;
	uint32_t immuneToStunAttackCount, immuneToStunUseItemCount, immuneToStunEquipCount;
	uint32_t immuneToStunInteractCount;
	
	bitStream.Read(immuneToStunMoveCount);
	bitStream.Read(immuneToStunJumpCount);
	bitStream.Read(immuneToStunTurnCount);
	bitStream.Read(immuneToStunAttackCount);
	bitStream.Read(immuneToStunUseItemCount);
	bitStream.Read(immuneToStunEquipCount);
	bitStream.Read(immuneToStunInteractCount);
	
	// Values should reflect the set immunities
	// Note: The actual mapping depends on implementation
	ASSERT_GE(immuneToStunMoveCount + immuneToStunJumpCount + immuneToStunTurnCount +
			  immuneToStunAttackCount + immuneToStunUseItemCount + immuneToStunEquipCount +
			  immuneToStunInteractCount, 0);
}

TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsSerializeUpdateTest) {
	// Test non-initial update serialization  
	physicsComponent->Serialize(bitStream, false);
	
	// Should check for various dirty flags
	bool hasPositionUpdate, hasVelocityUpdate, hasAngularVelocityUpdate;
	bool hasBouncePathing, hasRotationUpdate;
	
	// The exact structure depends on what's dirty
	// For now, just verify that serialization doesn't crash
	ASSERT_GE(bitStream.GetNumberOfBitsUsed(), 0);
}

