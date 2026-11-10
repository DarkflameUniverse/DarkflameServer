#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "SimplePhysicsComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class SimplePhysicsTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	SimplePhysicsComponent* simplePhysicsComponent;
	RakNet::BitStream bitStream;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		simplePhysicsComponent = baseEntity->AddComponent<SimplePhysicsComponent>(1);
		simplePhysicsComponent->SetClimbableType(eClimbableType::CLIMBABLE_TYPE_WALL);
		simplePhysicsComponent->SetPosition(NiPoint3(1.0f, 2.0f, 3.0f));
		simplePhysicsComponent->SetRotation(NiQuaternion(1.0f, 2.0f, 3.0f, 4.0f));
		simplePhysicsComponent->SetVelocity(NiPoint3(5.0f, 6.0f, 7.0f));
		simplePhysicsComponent->SetAngularVelocity(NiPoint3(5.0f, 6.0f, 7.0f));
		simplePhysicsComponent->SetPhysicsMotionState(2);
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(SimplePhysicsTest, SimplePhysicsSerializeTest) {
	simplePhysicsComponent->Serialize(bitStream, false);
	constexpr uint32_t sizeOfStream = 3 + BYTES_TO_BITS(3 * sizeof(NiPoint3)) + BYTES_TO_BITS(1 * sizeof(NiQuaternion)) + 1 * BYTES_TO_BITS(sizeof(uint32_t));
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), sizeOfStream);
	
	bool dirtyVelocityFlag;
	bitStream.Read(dirtyVelocityFlag);
	ASSERT_EQ(dirtyVelocityFlag, true);

	NiPoint3 velocity;
	bitStream.Read(velocity.x);
	bitStream.Read(velocity.y);
	bitStream.Read(velocity.z);
	ASSERT_EQ(velocity, NiPoint3(5.0f, 6.0f, 7.0f));

	NiPoint3 angularVelocity;
	bitStream.Read(angularVelocity.x);
	bitStream.Read(angularVelocity.y);
	bitStream.Read(angularVelocity.z);
	ASSERT_EQ(angularVelocity, NiPoint3(5.0f, 6.0f, 7.0f));

	bool dirtyPhysicsMotionStateFlag;
	bitStream.Read(dirtyPhysicsMotionStateFlag);
	ASSERT_EQ(dirtyPhysicsMotionStateFlag, true);

	uint32_t physicsMotionState;
	bitStream.Read(physicsMotionState);
	ASSERT_EQ(physicsMotionState, 2.0f);

	bool dirtyPositionFlag;
	bitStream.Read(dirtyPositionFlag);
	ASSERT_EQ(dirtyPositionFlag, true);

	NiPoint3 position;
	bitStream.Read(position.x);
	bitStream.Read(position.y);
	bitStream.Read(position.z);
	ASSERT_EQ(position, NiPoint3(1.0f, 2.0f, 3.0f));

	NiQuaternion rotation;
	bitStream.Read(rotation.x);
	bitStream.Read(rotation.y);
	bitStream.Read(rotation.z);
	bitStream.Read(rotation.w);
	ASSERT_EQ(rotation, NiQuaternion(1.0f, 2.0f, 3.0f, 4.0f));
}

TEST_F(SimplePhysicsTest, SimplePhysicsConstructionTest) {
	simplePhysicsComponent->Serialize(bitStream, true);
	constexpr uint32_t sizeOfStream = 4 + BYTES_TO_BITS(1 * sizeof(int32_t)) + BYTES_TO_BITS(3 * sizeof(NiPoint3)) + BYTES_TO_BITS(1 * sizeof(NiQuaternion)) + 1 * BYTES_TO_BITS(sizeof(uint32_t));
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), sizeOfStream);

	bool dirtyClimbableTypeFlag;
	bitStream.Read(dirtyClimbableTypeFlag);
	ASSERT_EQ(dirtyClimbableTypeFlag, true);

	int32_t climbableType;
	bitStream.Read(climbableType);
	ASSERT_EQ(climbableType, 2);

	bool dirtyVelocityFlag;
	bitStream.Read(dirtyVelocityFlag);
	ASSERT_EQ(dirtyVelocityFlag, true);

	NiPoint3 velocity;
	bitStream.Read(velocity.x);
	bitStream.Read(velocity.y);
	bitStream.Read(velocity.z);
	ASSERT_EQ(velocity, NiPoint3(5.0f, 6.0f, 7.0f));
	
	NiPoint3 angularVelocity;
	bitStream.Read(angularVelocity.x);
	bitStream.Read(angularVelocity.y);
	bitStream.Read(angularVelocity.z);
	ASSERT_EQ(angularVelocity, NiPoint3(5.0f, 6.0f, 7.0f));

	bool dirtyPhysicsMotionStateFlag;
	bitStream.Read(dirtyPhysicsMotionStateFlag);
	ASSERT_EQ(dirtyPhysicsMotionStateFlag, true);

	uint32_t physicsMotionState;
	bitStream.Read(physicsMotionState);
	ASSERT_EQ(physicsMotionState, 2.0f);

	bool dirtyPositionFlag;
	bitStream.Read(dirtyPositionFlag);
	ASSERT_EQ(dirtyPositionFlag, true);

	NiPoint3 position;
	bitStream.Read(position.x);
	bitStream.Read(position.y);
	bitStream.Read(position.z);
	ASSERT_EQ(position, NiPoint3(1.0f, 2.0f, 3.0f));

	NiQuaternion rotation;
	bitStream.Read(rotation.x);
	bitStream.Read(rotation.y);
	bitStream.Read(rotation.z);
	bitStream.Read(rotation.w);
	ASSERT_EQ(rotation, NiQuaternion(1.0f, 2.0f, 3.0f, 4.0f));
}

// A construction (the object coming into one player's range) doesn't use up a velocity or motion state change the
// players already watching it still have to get. LWOSimplePhysicsComponent::Deserialize (0x00be4b00) reads each
// behind its own flag bit.
TEST_F(SimplePhysicsTest, AConstructionKeepsPendingChangesForTheNextUpdate) {
	RakNet::BitStream construction;
	simplePhysicsComponent->Serialize(construction, true);

	simplePhysicsComponent->Serialize(bitStream, false);
	bool hasVelocity{};
	bitStream.Read(hasVelocity);
	ASSERT_TRUE(hasVelocity);
	NiPoint3 velocity;
	NiPoint3 angularVelocity;
	bitStream.Read(velocity);
	bitStream.Read(angularVelocity);
	EXPECT_EQ(velocity, NiPoint3(5.0f, 6.0f, 7.0f));
	bool hasMotionState{};
	bitStream.Read(hasMotionState);
	ASSERT_TRUE(hasMotionState);
	uint32_t motionState{};
	bitStream.Read(motionState);
	EXPECT_EQ(motionState, 2u);

	// Written once
	RakNet::BitStream again;
	simplePhysicsComponent->Serialize(again, false);
	EXPECT_EQ(again.GetNumberOfBitsUsed(), 3u);
}

TEST_F(SimplePhysicsTest, SimplePhysicsGettersAndSettersTest) {
	ASSERT_EQ(simplePhysicsComponent->GetClimabbleType(), eClimbableType::CLIMBABLE_TYPE_WALL);
	ASSERT_EQ(simplePhysicsComponent->GetPosition(), NiPoint3(1.0f, 2.0f, 3.0f));
	ASSERT_EQ(simplePhysicsComponent->GetRotation(), NiQuaternion(1.0f, 2.0f, 3.0f, 4.0f));
	ASSERT_EQ(simplePhysicsComponent->GetVelocity(), NiPoint3(5.0f, 6.0f, 7.0f));
	ASSERT_EQ(simplePhysicsComponent->GetAngularVelocity(), NiPoint3(5.0f, 6.0f, 7.0f));
	ASSERT_EQ(simplePhysicsComponent->GetPhysicsMotionState(), 2);
	simplePhysicsComponent->SetClimbableType(eClimbableType::CLIMBABLE_TYPE_LADDER);
	simplePhysicsComponent->SetPosition(NiPoint3(4.0f, 5.0f, 6.0f));
	simplePhysicsComponent->SetRotation(NiQuaternion(4.0f, 5.0f, 6.0f, 7.0f));
	simplePhysicsComponent->SetVelocity(NiPoint3(6.0f, 7.0f, 8.0f));
	simplePhysicsComponent->SetAngularVelocity(NiPoint3(6.0f, 7.0f, 8.0f));
	simplePhysicsComponent->SetPhysicsMotionState(3);
	ASSERT_EQ(simplePhysicsComponent->GetClimabbleType(), eClimbableType::CLIMBABLE_TYPE_LADDER);
	ASSERT_EQ(simplePhysicsComponent->GetPosition(), NiPoint3(4.0f, 5.0f, 6.0f));
	ASSERT_EQ(simplePhysicsComponent->GetRotation(), NiQuaternion(4.0f, 5.0f, 6.0f, 7.0f));
	ASSERT_EQ(simplePhysicsComponent->GetVelocity(), NiPoint3(6.0f, 7.0f, 8.0f));
	ASSERT_EQ(simplePhysicsComponent->GetAngularVelocity(), NiPoint3(6.0f, 7.0f, 8.0f));
	ASSERT_EQ(simplePhysicsComponent->GetPhysicsMotionState(), 3);
}
