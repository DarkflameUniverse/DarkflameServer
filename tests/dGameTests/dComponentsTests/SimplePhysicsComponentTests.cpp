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
	CBITSTREAM;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		simplePhysicsComponent = new SimplePhysicsComponent(1, baseEntity.get());
		baseEntity->AddComponent(SimplePhysicsComponent::ComponentType, simplePhysicsComponent);
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
	simplePhysicsComponent->Serialize(&bitStream, false);
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
