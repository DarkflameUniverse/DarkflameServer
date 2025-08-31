#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "HavokVehiclePhysicsComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "PositionUpdate.h"

class HavokVehiclePhysicsTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	HavokVehiclePhysicsComponent* havokVehiclePhysicsComponent;
	CBITSTREAM
		uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		havokVehiclePhysicsComponent = baseEntity->AddComponent<HavokVehiclePhysicsComponent>(1);
		// Initialize some values to be not default
		havokVehiclePhysicsComponent->SetPosition(NiPoint3(10.0f, 20.0f, 30.0f));
		havokVehiclePhysicsComponent->SetRotation(NiQuaternion(4.0f, 1.0f, 2.0f, 3.0f));
		havokVehiclePhysicsComponent->SetVelocity(NiPoint3(5.0f, 6.0f, 7.0f));
		havokVehiclePhysicsComponent->SetAngularVelocity(NiPoint3(8.0f, 9.0f, 10.0f));
		havokVehiclePhysicsComponent->SetIsOnGround(true);
		havokVehiclePhysicsComponent->SetIsOnRail(false);
		
		// Set remote input info
		RemoteInputInfo remoteInput;
		remoteInput.m_RemoteInputX = 0.5f;
		remoteInput.m_RemoteInputY = -0.3f;
		remoteInput.m_IsPowersliding = true;
		remoteInput.m_IsModified = false;
		havokVehiclePhysicsComponent->SetRemoteInputInfo(remoteInput);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of HavokVehiclePhysicsComponent during initial update
 */
TEST_F(HavokVehiclePhysicsTest, HavokVehiclePhysicsComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Now we test a serialization for correctness with initial update.
	havokVehiclePhysicsComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	
	// First, check if position data is written
	bool hasPositionData;
	bitStream.Read(hasPositionData);
	EXPECT_EQ(hasPositionData, true);
	
	if (hasPositionData) {
		// Position
		float posX, posY, posZ;
		bitStream.Read(posX);
		bitStream.Read(posY);
		bitStream.Read(posZ);
		EXPECT_EQ(posX, 10.0f);
		EXPECT_EQ(posY, 20.0f);
		EXPECT_EQ(posZ, 30.0f);
		
		// Rotation (x, y, z, w order)
		float rotX, rotY, rotZ, rotW;
		bitStream.Read(rotX);
		bitStream.Read(rotY);
		bitStream.Read(rotZ);
		bitStream.Read(rotW);
		EXPECT_EQ(rotX, 1.0f);
		EXPECT_EQ(rotY, 2.0f);
		EXPECT_EQ(rotZ, 3.0f);
		EXPECT_EQ(rotW, 4.0f);
		
		// Ground and rail status
		bool isOnGround, isOnRail;
		bitStream.Read(isOnGround);
		bitStream.Read(isOnRail);
		EXPECT_EQ(isOnGround, true);
		EXPECT_EQ(isOnRail, false);
		
		// Velocity (conditional)
		bool hasVelocity;
		bitStream.Read(hasVelocity);
		EXPECT_EQ(hasVelocity, true);
		
		if (hasVelocity) {
			float velX, velY, velZ;
			bitStream.Read(velX);
			bitStream.Read(velY);
			bitStream.Read(velZ);
			EXPECT_EQ(velX, 5.0f);
			EXPECT_EQ(velY, 6.0f);
			EXPECT_EQ(velZ, 7.0f);
		}
		
		// Angular velocity (conditional)
		bool hasAngularVelocity;
		bitStream.Read(hasAngularVelocity);
		EXPECT_EQ(hasAngularVelocity, true);
		
		if (hasAngularVelocity) {
			float angVelX, angVelY, angVelZ;
			bitStream.Read(angVelX);
			bitStream.Read(angVelY);
			bitStream.Read(angVelZ);
			EXPECT_EQ(angVelX, 8.0f);
			EXPECT_EQ(angVelY, 9.0f);
			EXPECT_EQ(angVelZ, 10.0f);
		}
		
		// Local space info (always false for now)
		bool hasLocalSpaceInfo;
		bitStream.Read(hasLocalSpaceInfo);
		EXPECT_EQ(hasLocalSpaceInfo, false);
		
		// Remote input info (always true)
		bool hasRemoteInputInfo;
		bitStream.Read(hasRemoteInputInfo);
		EXPECT_EQ(hasRemoteInputInfo, true);
		
		if (hasRemoteInputInfo) {
			float remoteInputX, remoteInputY;
			bool isPowersliding, isModified;
			bitStream.Read(remoteInputX);
			bitStream.Read(remoteInputY);
			bitStream.Read(isPowersliding);
			bitStream.Read(isModified);
			
			EXPECT_EQ(remoteInputX, 0.5f);
			EXPECT_EQ(remoteInputY, -0.3f);
			EXPECT_EQ(isPowersliding, true);
			EXPECT_EQ(isModified, false);
		}
		
		// Remote input ping (always 125.0f)
		float remoteInputPing;
		bitStream.Read(remoteInputPing);
		EXPECT_EQ(remoteInputPing, 125.0f);
	}
	
	// Initial update specific data
	uint8_t endBehavior;
	bitStream.Read(endBehavior);
	EXPECT_GE(endBehavior, 0); // Generated randomly between 0-7
	EXPECT_LE(endBehavior, 7);
	
	bool isInputLocked;
	bitStream.Read(isInputLocked);
	EXPECT_EQ(isInputLocked, true); // Always true
	
	// Final flag (always false)
	bool finalFlag;
	bitStream.Read(finalFlag);
	EXPECT_EQ(finalFlag, false);
}

/**
 * Test serialization of HavokVehiclePhysicsComponent during regular update
 */
TEST_F(HavokVehiclePhysicsTest, HavokVehiclePhysicsComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Now we test a serialization for correctness with regular update.
	havokVehiclePhysicsComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	
	// First, check if position data is written
	bool hasPositionData;
	bitStream.Read(hasPositionData);
	EXPECT_EQ(hasPositionData, true);
	
	if (hasPositionData) {
		// Position
		float posX, posY, posZ;
		bitStream.Read(posX);
		bitStream.Read(posY);
		bitStream.Read(posZ);
		EXPECT_EQ(posX, 10.0f);
		EXPECT_EQ(posY, 20.0f);
		EXPECT_EQ(posZ, 30.0f);
		
		// Rotation (x, y, z, w order)
		float rotX, rotY, rotZ, rotW;
		bitStream.Read(rotX);
		bitStream.Read(rotY);
		bitStream.Read(rotZ);
		bitStream.Read(rotW);
		EXPECT_EQ(rotX, 1.0f);
		EXPECT_EQ(rotY, 2.0f);
		EXPECT_EQ(rotZ, 3.0f);
		EXPECT_EQ(rotW, 4.0f);
		
		// Ground and rail status
		bool isOnGround, isOnRail;
		bitStream.Read(isOnGround);
		bitStream.Read(isOnRail);
		EXPECT_EQ(isOnGround, true);
		EXPECT_EQ(isOnRail, false);
		
		// Velocity (conditional)
		bool hasVelocity;
		bitStream.Read(hasVelocity);
		EXPECT_EQ(hasVelocity, true);
		
		if (hasVelocity) {
			float velX, velY, velZ;
			bitStream.Read(velX);
			bitStream.Read(velY);
			bitStream.Read(velZ);
			EXPECT_EQ(velX, 5.0f);
			EXPECT_EQ(velY, 6.0f);
			EXPECT_EQ(velZ, 7.0f);
		}
		
		// Angular velocity (conditional)
		bool hasAngularVelocity;
		bitStream.Read(hasAngularVelocity);
		EXPECT_EQ(hasAngularVelocity, true);
		
		if (hasAngularVelocity) {
			float angVelX, angVelY, angVelZ;
			bitStream.Read(angVelX);
			bitStream.Read(angVelY);
			bitStream.Read(angVelZ);
			EXPECT_EQ(angVelX, 8.0f);
			EXPECT_EQ(angVelY, 9.0f);
			EXPECT_EQ(angVelZ, 10.0f);
		}
		
		// Local space info (always false for now)
		bool hasLocalSpaceInfo;
		bitStream.Read(hasLocalSpaceInfo);
		EXPECT_EQ(hasLocalSpaceInfo, false);
		
		// Remote input info (always true)
		bool hasRemoteInputInfo;
		bitStream.Read(hasRemoteInputInfo);
		EXPECT_EQ(hasRemoteInputInfo, true);
		
		if (hasRemoteInputInfo) {
			float remoteInputX, remoteInputY;
			bool isPowersliding, isModified;
			bitStream.Read(remoteInputX);
			bitStream.Read(remoteInputY);
			bitStream.Read(isPowersliding);
			bitStream.Read(isModified);
			
			EXPECT_EQ(remoteInputX, 0.5f);
			EXPECT_EQ(remoteInputY, -0.3f);
			EXPECT_EQ(isPowersliding, true);
			EXPECT_EQ(isModified, false);
		}
		
		// Remote input ping (always 125.0f)
		float remoteInputPing;
		bitStream.Read(remoteInputPing);
		EXPECT_EQ(remoteInputPing, 125.0f);
		
		// Regular update has an additional flag
		bool extraFlag;
		bitStream.Read(extraFlag);
		EXPECT_EQ(extraFlag, false);
	}
	
	// Final flag (always false)
	bool finalFlag;
	bitStream.Read(finalFlag);
	EXPECT_EQ(finalFlag, false);
}

/**
 * Test serialization with zero velocities
 */
TEST_F(HavokVehiclePhysicsTest, HavokVehiclePhysicsComponentSerializeZeroVelocitiesTest) {
	bitStream.Reset();
	
	// Set velocities to zero
	havokVehiclePhysicsComponent->SetVelocity(NiPoint3(0.0f, 0.0f, 0.0f));
	havokVehiclePhysicsComponent->SetAngularVelocity(NiPoint3(0.0f, 0.0f, 0.0f));
	
	// Now we test a serialization for correctness.
	havokVehiclePhysicsComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	
	// First, check if position data is written
	bool hasPositionData;
	bitStream.Read(hasPositionData);
	EXPECT_EQ(hasPositionData, true);
	
	if (hasPositionData) {
		// Skip position and rotation data
		float posX, posY, posZ;
		bitStream.Read(posX);
		bitStream.Read(posY);
		bitStream.Read(posZ);
		
		float rotX, rotY, rotZ, rotW;
		bitStream.Read(rotX);
		bitStream.Read(rotY);
		bitStream.Read(rotZ);
		bitStream.Read(rotW);
		
		bool isOnGround, isOnRail;
		bitStream.Read(isOnGround);
		bitStream.Read(isOnRail);
		
		// Velocity should be false since it's zero
		bool hasVelocity;
		bitStream.Read(hasVelocity);
		EXPECT_EQ(hasVelocity, false);
		
		// Angular velocity should be false since it's zero
		bool hasAngularVelocity;
		bitStream.Read(hasAngularVelocity);
		EXPECT_EQ(hasAngularVelocity, false);
		
		// Continue with rest of serialization...
		bool hasLocalSpaceInfo;
		bitStream.Read(hasLocalSpaceInfo);
		EXPECT_EQ(hasLocalSpaceInfo, false);
		
		bool hasRemoteInputInfo;
		bitStream.Read(hasRemoteInputInfo);
		EXPECT_EQ(hasRemoteInputInfo, true);
	}
}

/**
 * Test HavokVehiclePhysicsComponent getters and setters
 */
TEST_F(HavokVehiclePhysicsTest, HavokVehiclePhysicsComponentGettersSettersTest) {
	// Test velocity
	NiPoint3 testVelocity(100.0f, 200.0f, 300.0f);
	havokVehiclePhysicsComponent->SetVelocity(testVelocity);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetVelocity(), testVelocity);
	
	// Test angular velocity
	NiPoint3 testAngularVelocity(50.0f, 60.0f, 70.0f);
	havokVehiclePhysicsComponent->SetAngularVelocity(testAngularVelocity);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetAngularVelocity(), testAngularVelocity);
	
	// Test ground state
	havokVehiclePhysicsComponent->SetIsOnGround(false);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetIsOnGround(), false);
	
	havokVehiclePhysicsComponent->SetIsOnGround(true);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetIsOnGround(), true);
	
	// Test rail state
	havokVehiclePhysicsComponent->SetIsOnRail(true);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetIsOnRail(), true);
	
	havokVehiclePhysicsComponent->SetIsOnRail(false);
	EXPECT_EQ(havokVehiclePhysicsComponent->GetIsOnRail(), false);
}