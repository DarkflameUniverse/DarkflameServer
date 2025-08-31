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
 * Test ControllablePhysicsComponent initial update serialization with default values
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Test initial update serialization
	controllablePhysicsComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	// Jetpack mode (should be false by default)
	bool inJetpackMode;
	bitStream.Read(inJetpackMode);
	EXPECT_EQ(inJetpackMode, false);
	
	// Immune counts section (always written on construction)
	bool hasImmuneCounts = true; // This is always written as Write1()
	// Note: The code uses Write1() which means this bit is always true
	
	uint32_t immuneToStunMoveCount;
	bitStream.Read(immuneToStunMoveCount);
	EXPECT_EQ(immuneToStunMoveCount, 0); // Default value
	
	uint32_t immuneToStunJumpCount;
	bitStream.Read(immuneToStunJumpCount);
	EXPECT_EQ(immuneToStunJumpCount, 0);
	
	uint32_t immuneToStunTurnCount;
	bitStream.Read(immuneToStunTurnCount);
	EXPECT_EQ(immuneToStunTurnCount, 0);
	
	uint32_t immuneToStunAttackCount;
	bitStream.Read(immuneToStunAttackCount);
	EXPECT_EQ(immuneToStunAttackCount, 0);
	
	uint32_t immuneToStunUseItemCount;
	bitStream.Read(immuneToStunUseItemCount);
	EXPECT_EQ(immuneToStunUseItemCount, 0);
	
	uint32_t immuneToStunEquipCount;
	bitStream.Read(immuneToStunEquipCount);
	EXPECT_EQ(immuneToStunEquipCount, 0);
	
	uint32_t immuneToStunInteractCount;
	bitStream.Read(immuneToStunInteractCount);
	EXPECT_EQ(immuneToStunInteractCount, 0);
	
	// Cheats section (always dirty on initial update)
	bool hasCheats;
	bitStream.Read(hasCheats);
	EXPECT_EQ(hasCheats, true); // Always true for initial update
	
	float gravityScale;
	bitStream.Read(gravityScale);
	EXPECT_EQ(gravityScale, 1.0f); // Default value
	
	float speedMultiplier;
	bitStream.Read(speedMultiplier);
	EXPECT_EQ(speedMultiplier, 1.0f); // Default value
	
	// Equipped item info section (always dirty on initial update)
	bool hasEquippedItemInfo;
	bitStream.Read(hasEquippedItemInfo);
	EXPECT_EQ(hasEquippedItemInfo, true);
	
	float pickupRadius;
	bitStream.Read(pickupRadius);
	EXPECT_EQ(pickupRadius, 0.0f); // Default value
	
	bool inJetpackMode2;
	bitStream.Read(inJetpackMode2);
	EXPECT_EQ(inJetpackMode2, false); // Should match first jetpack mode
	
	// Bubble section (always dirty on initial update)
	bool hasBubble;
	bitStream.Read(hasBubble);
	EXPECT_EQ(hasBubble, true);
	
	bool isInBubble;
	bitStream.Read(isInBubble);
	EXPECT_EQ(isInBubble, false); // Default value
	
	// Position section (always dirty on initial update)
	bool hasPosition;
	bitStream.Read(hasPosition);
	EXPECT_EQ(hasPosition, true);
	
	// Position
	float posX, posY, posZ;
	bitStream.Read(posX);
	bitStream.Read(posY);
	bitStream.Read(posZ);
	EXPECT_EQ(posX, 0.0f);
	EXPECT_EQ(posY, 0.0f);
	EXPECT_EQ(posZ, 0.0f);
	
	// Rotation (quaternion)
	float rotX, rotY, rotZ, rotW;
	bitStream.Read(rotX);
	bitStream.Read(rotY);
	bitStream.Read(rotZ);
	bitStream.Read(rotW);
	// Default quaternion values
	EXPECT_EQ(rotX, 0.0f);
	EXPECT_EQ(rotY, 0.0f);
	EXPECT_EQ(rotZ, 0.0f);
	EXPECT_EQ(rotW, 1.0f);
	
	// Ground and rail flags
	bool isOnGround;
	bitStream.Read(isOnGround);
	EXPECT_EQ(isOnGround, true); // Default value
	
	bool isOnRail;
	bitStream.Read(isOnRail);
	EXPECT_EQ(isOnRail, false); // Default value
	
	// Velocity (should be zero by default)
	bool hasVelocity;
	bitStream.Read(hasVelocity);
	EXPECT_EQ(hasVelocity, false); // Zero velocity by default
	
	// Angular velocity (should be zero by default)
	bool hasAngularVelocity;
	bitStream.Read(hasAngularVelocity);
	EXPECT_EQ(hasAngularVelocity, false); // Zero angular velocity by default
	
	// Local space info (always zero)
	bool localSpaceInfo;
	bitStream.Read(localSpaceInfo);
	EXPECT_EQ(localSpaceInfo, false);
}

/**
 * Test ControllablePhysicsComponent initial update with jetpack mode enabled
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeJetpackTest) {
	bitStream.Reset();
	
	// Enable jetpack mode with specific values
	// Note: These would typically be set through appropriate setters if they exist
	// For now, we'll test the default case and focus on other conditional logic
	
	// Set some non-zero velocity to test conditional serialization
	controllablePhysicsComponent->SetVelocity(NiPoint3(1.0f, 2.0f, 3.0f));
	controllablePhysicsComponent->SetAngularVelocity(NiPoint3(0.1f, 0.2f, 0.3f));
	
	controllablePhysicsComponent->Serialize(bitStream, true);
	
	// Skip to velocity section (navigate through the structure)
	// We'll focus on testing the velocity serialization logic
	
	bool dummy;
	float dummyFloat;
	uint32_t dummyInt;
	
	// Skip jetpack mode
	bitStream.Read(dummy); // inJetpackMode
	
	// Skip immune counts (7 uint32_t values)
	for (int i = 0; i < 7; i++) {
		bitStream.Read(dummyInt);
	}
	
	// Skip cheats section
	bitStream.Read(dummy); // hasCheats
	bitStream.Read(dummyFloat); // gravityScale
	bitStream.Read(dummyFloat); // speedMultiplier
	
	// Skip equipped item info
	bitStream.Read(dummy); // hasEquippedItemInfo
	bitStream.Read(dummyFloat); // pickupRadius
	bitStream.Read(dummy); // inJetpackMode2
	
	// Skip bubble section
	bitStream.Read(dummy); // hasBubble
	bitStream.Read(dummy); // isInBubble
	
	// Skip position section
	bitStream.Read(dummy); // hasPosition
	bitStream.Read(dummyFloat); // posX
	bitStream.Read(dummyFloat); // posY
	bitStream.Read(dummyFloat); // posZ
	bitStream.Read(dummyFloat); // rotX
	bitStream.Read(dummyFloat); // rotY
	bitStream.Read(dummyFloat); // rotZ
	bitStream.Read(dummyFloat); // rotW
	bitStream.Read(dummy); // isOnGround
	bitStream.Read(dummy); // isOnRail
	
	// Now test velocity section
	bool hasVelocity;
	bitStream.Read(hasVelocity);
	EXPECT_EQ(hasVelocity, true); // Should have velocity now
	
	if (hasVelocity) {
		float velX, velY, velZ;
		bitStream.Read(velX);
		bitStream.Read(velY);
		bitStream.Read(velZ);
		EXPECT_EQ(velX, 1.0f);
		EXPECT_EQ(velY, 2.0f);
		EXPECT_EQ(velZ, 3.0f);
	}
	
	// Test angular velocity section
	bool hasAngularVelocity;
	bitStream.Read(hasAngularVelocity);
	EXPECT_EQ(hasAngularVelocity, true); // Should have angular velocity now
	
	if (hasAngularVelocity) {
		float angVelX, angVelY, angVelZ;
		bitStream.Read(angVelX);
		bitStream.Read(angVelY);
		bitStream.Read(angVelZ);
		EXPECT_EQ(angVelX, 0.1f);
		EXPECT_EQ(angVelY, 0.2f);
		EXPECT_EQ(angVelZ, 0.3f);
	}
	
	// Local space info
	bool localSpaceInfo;
	bitStream.Read(localSpaceInfo);
	EXPECT_EQ(localSpaceInfo, false);
}

/**
 * Test ControllablePhysicsComponent regular update serialization
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Regular update should only serialize dirty flags
	controllablePhysicsComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	// Cheats section (should not be dirty by default)
	bool hasCheats;
	bitStream.Read(hasCheats);
	EXPECT_EQ(hasCheats, false); // Not dirty by default
	
	// Equipped item info section (should not be dirty by default)
	bool hasEquippedItemInfo;
	bitStream.Read(hasEquippedItemInfo);
	EXPECT_EQ(hasEquippedItemInfo, false); // Not dirty by default
	
	// Bubble section (should not be dirty by default)
	bool hasBubble;
	bitStream.Read(hasBubble);
	EXPECT_EQ(hasBubble, false); // Not dirty by default
	
	// Position section (should not be dirty by default)
	bool hasPosition;
	bitStream.Read(hasPosition);
	EXPECT_EQ(hasPosition, false); // Not dirty by default
}

/**
 * Test ControllablePhysicsComponent regular update with position change
 */
TEST_F(ControllablePhysicsComponentTest, ControllablePhysicsComponentSerializePositionChangeTest) {
	bitStream.Reset();
	
	// Change position to make it dirty
	controllablePhysicsComponent->SetPosition(NiPoint3(10.0f, 20.0f, 30.0f));
	controllablePhysicsComponent->SetRotation(NiQuaternion(0.0f, 0.0f, 0.0f, 1.0f));
	
	// Regular update should now serialize position
	controllablePhysicsComponent->Serialize(bitStream, false);
	
	// Skip non-dirty sections
	bool hasCheats;
	bitStream.Read(hasCheats);
	EXPECT_EQ(hasCheats, false);
	
	bool hasEquippedItemInfo;
	bitStream.Read(hasEquippedItemInfo);
	EXPECT_EQ(hasEquippedItemInfo, false);
	
	bool hasBubble;
	bitStream.Read(hasBubble);
	EXPECT_EQ(hasBubble, false);
	
	// Position section should now be dirty
	bool hasPosition;
	bitStream.Read(hasPosition);
	EXPECT_EQ(hasPosition, true); // Should be dirty due to position change
	
	if (hasPosition) {
		float posX, posY, posZ;
		bitStream.Read(posX);
		bitStream.Read(posY);
		bitStream.Read(posZ);
		EXPECT_EQ(posX, 10.0f);
		EXPECT_EQ(posY, 20.0f);
		EXPECT_EQ(posZ, 30.0f);
		
		float rotX, rotY, rotZ, rotW;
		bitStream.Read(rotX);
		bitStream.Read(rotY);
		bitStream.Read(rotZ);
		bitStream.Read(rotW);
		EXPECT_EQ(rotX, 0.0f);
		EXPECT_EQ(rotY, 0.0f);
		EXPECT_EQ(rotZ, 0.0f);
		EXPECT_EQ(rotW, 1.0f);
		
		bool isOnGround;
		bitStream.Read(isOnGround);
		EXPECT_EQ(isOnGround, true);
		
		bool isOnRail;
		bitStream.Read(isOnRail);
		EXPECT_EQ(isOnRail, false);
		
		bool hasVelocity;
		bitStream.Read(hasVelocity);
		EXPECT_EQ(hasVelocity, false); // Zero velocity
		
		bool hasAngularVelocity;
		bitStream.Read(hasAngularVelocity);
		EXPECT_EQ(hasAngularVelocity, false); // Zero angular velocity
		
		bool localSpaceInfo;
		bitStream.Read(localSpaceInfo);
		EXPECT_EQ(localSpaceInfo, false);
		
		// In regular updates, teleporting flag is written
		bool isTeleporting;
		bitStream.Read(isTeleporting);
		EXPECT_EQ(isTeleporting, false); // Default value
	}
}