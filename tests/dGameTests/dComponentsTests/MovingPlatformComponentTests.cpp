#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"

#define _MOVING_PLATFORM_TEST
#include "MovingPlatformComponent.h"
#undef _MOVING_PLATFORM_TEST
#include "eReplicaComponentType.h"

class MovingPlatformComponentTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	CBITSTREAM;
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		auto* movingPlatformComponent = new MovingPlatformComponent(baseEntity.get(), "");
		baseEntity->AddComponent(eReplicaComponentType::MOVING_PLATFORM, movingPlatformComponent);
		auto moverPlatformSubComponent = std::make_unique<MoverPlatformSubComponent>();
		moverPlatformSubComponent->m_State = eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint;
		moverPlatformSubComponent->m_DesiredWaypointIndex = 1;
		moverPlatformSubComponent->m_PercentBetweenPoints = 2;
		moverPlatformSubComponent->m_Position = NiPoint3(3, 4, 5);
		moverPlatformSubComponent->m_CurrentWaypointIndex = 6;
		moverPlatformSubComponent->m_NextWaypointIndex = 7;
		moverPlatformSubComponent->m_IdleTimeElapsed = 8;
		moverPlatformSubComponent->m_MoveTimeElapsed = 11;
		moverPlatformSubComponent->m_IsDirty = true;
		moverPlatformSubComponent->m_InReverse = true;
		moverPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;

		auto rotatorPlatformSubComponent = std::make_unique<RotatorPlatformSubComponent>();
		rotatorPlatformSubComponent->m_State = eMovementPlatformState::Travelling;
		rotatorPlatformSubComponent->m_DesiredWaypointIndex = 12;
		rotatorPlatformSubComponent->m_PercentBetweenPoints = 13;
		rotatorPlatformSubComponent->m_Position = NiPoint3(14, 15, 16);
		rotatorPlatformSubComponent->m_CurrentWaypointIndex = 17;
		rotatorPlatformSubComponent->m_NextWaypointIndex = 18;
		rotatorPlatformSubComponent->m_IdleTimeElapsed = 19;
		rotatorPlatformSubComponent->m_MoveTimeElapsed = 22;
		rotatorPlatformSubComponent->m_IsDirty = true;
		rotatorPlatformSubComponent->m_InReverse = true;
		rotatorPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;

		auto simpleMoverPlatformSubComponent = std::make_unique<SimpleMoverPlatformSubComponent>();
		simpleMoverPlatformSubComponent->m_State = eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint;
		simpleMoverPlatformSubComponent->m_DesiredWaypointIndex = 23;
		simpleMoverPlatformSubComponent->m_PercentBetweenPoints = 24;
		simpleMoverPlatformSubComponent->m_Position = NiPoint3(25, 26, 27);
		simpleMoverPlatformSubComponent->m_CurrentWaypointIndex = 28;
		simpleMoverPlatformSubComponent->m_NextWaypointIndex = 29;
		simpleMoverPlatformSubComponent->m_IdleTimeElapsed = 30;
		simpleMoverPlatformSubComponent->m_MoveTimeElapsed = 33;
		simpleMoverPlatformSubComponent->m_IsDirty = true;
		simpleMoverPlatformSubComponent->m_InReverse = true;
		simpleMoverPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;

		movingPlatformComponent->_AddPlatformSubComponent(std::move(moverPlatformSubComponent));
		movingPlatformComponent->_AddPlatformSubComponent(std::move(rotatorPlatformSubComponent));
		movingPlatformComponent->_AddPlatformSubComponent(std::move(simpleMoverPlatformSubComponent));
		movingPlatformComponent->_SetPath(u"ExamplePath");
	}

	void TearDown() override {
		TearDownDependencies();
	}

	void TestSerialization() {
		auto* movingPlatformComponent = baseEntity->GetComponent<MovingPlatformComponent>();
		ASSERT_NE(movingPlatformComponent, nullptr);
		uint32_t flags = 0;
		movingPlatformComponent->Serialize(&bitStream, true, flags);
		// read in the full BitStream and check the values match what they were set to above
		bool hasPlatformSubComponents = false;
		bitStream.Read(hasPlatformSubComponents);
		ASSERT_TRUE(hasPlatformSubComponents);

		bool dirtyPathInfo;
		bitStream.Read(dirtyPathInfo);
		ASSERT_TRUE(dirtyPathInfo);

		bool hasPath;
		bitStream.Read(hasPath);
		ASSERT_TRUE(hasPath);

		std::u16string pathName;
		uint16_t pathNameLength;
		bitStream.Read(pathNameLength);
		pathName.resize(pathNameLength);
		bitStream.ReadBits(reinterpret_cast<unsigned char*>(pathName.data()), BYTES_TO_BITS(pathNameLength) * 2);
		ASSERT_EQ(pathName, u"ExamplePath");

		uint32_t pathStartIndex;
		bitStream.Read(pathStartIndex);
		ASSERT_EQ(pathStartIndex, 1);

		bool isInReverse;
		bitStream.Read(isInReverse);
		ASSERT_TRUE(isInReverse);

		bool hasPlatformData;
		bitStream.Read(hasPlatformData);
		ASSERT_TRUE(hasPlatformData);

		eMoverSubComponentType platformType;
		bitStream.Read(platformType);
		ASSERT_EQ(platformType, eMoverSubComponentType::Mover);

		bool isDirty;
		bitStream.Read(isDirty);
		ASSERT_TRUE(isDirty);

		eMovementPlatformState state;
		bitStream.Read(state);
		ASSERT_EQ(state, eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint);

		int32_t desiredWaypointIndex;
		bitStream.Read(desiredWaypointIndex);
		ASSERT_EQ(desiredWaypointIndex, 1);

		bool shouldStopAtDesiredWaypoint;
		bitStream.Read(shouldStopAtDesiredWaypoint);
		ASSERT_TRUE(shouldStopAtDesiredWaypoint);

		bool isInReverse2;
		bitStream.Read(isInReverse2);
		ASSERT_TRUE(isInReverse2);

		float percentBetweenPoints;
		bitStream.Read(percentBetweenPoints);
		ASSERT_EQ(percentBetweenPoints, 2);

		NiPoint3 position;
		bitStream.Read(position.x);
		bitStream.Read(position.y);
		bitStream.Read(position.z);
		ASSERT_EQ(position, NiPoint3(3, 4, 5));

		uint32_t currentWaypointIndex;
		bitStream.Read(currentWaypointIndex);
		ASSERT_EQ(currentWaypointIndex, 6);

		uint32_t nextWaypointIndex;
		bitStream.Read(nextWaypointIndex);
		ASSERT_EQ(nextWaypointIndex, 7);

		float idleTimeElapsed;
		bitStream.Read(idleTimeElapsed);
		ASSERT_EQ(idleTimeElapsed, 8);

		float moveTimeElapsed;
		bitStream.Read(moveTimeElapsed);
		ASSERT_EQ(moveTimeElapsed, 11);

		bool hasPlatformData2;
		bitStream.Read(hasPlatformData2);
		ASSERT_TRUE(hasPlatformData2);

		eMoverSubComponentType platformType2;
		bitStream.Read(platformType2);
		ASSERT_EQ(platformType2, eMoverSubComponentType::Rotator);

		bool isDirty2;
		bitStream.Read(isDirty2);
		ASSERT_TRUE(isDirty2);

		eMovementPlatformState state2;
		bitStream.Read(state2);
		ASSERT_EQ(state2, eMovementPlatformState::Travelling);

		int32_t desiredWaypointIndex2;
		bitStream.Read(desiredWaypointIndex2);
		ASSERT_EQ(desiredWaypointIndex2, 12);

		bool shouldStopAtDesiredWaypoint2;
		bitStream.Read(shouldStopAtDesiredWaypoint2);
		ASSERT_TRUE(shouldStopAtDesiredWaypoint2);

		bool isInReverse3;
		bitStream.Read(isInReverse3);
		ASSERT_TRUE(isInReverse3);

		float percentBetweenPoints2;
		bitStream.Read(percentBetweenPoints2);
		ASSERT_EQ(percentBetweenPoints2, 13);

		NiPoint3 position2;
		bitStream.Read(position2.x);
		bitStream.Read(position2.y);
		bitStream.Read(position2.z);
		ASSERT_EQ(position2, NiPoint3(14, 15, 16));

		uint32_t currentWaypointIndex2;
		bitStream.Read(currentWaypointIndex2);
		ASSERT_EQ(currentWaypointIndex2, 17);

		uint32_t nextWaypointIndex2;
		bitStream.Read(nextWaypointIndex2);
		ASSERT_EQ(nextWaypointIndex2, 18);

		float idleTimeElapsed2;
		bitStream.Read(idleTimeElapsed2);
		ASSERT_EQ(idleTimeElapsed2, 19);

		float moveTimeElapsed2;
		bitStream.Read(moveTimeElapsed2);
		ASSERT_EQ(moveTimeElapsed2, 22);

		bool hasPlatformData3;
		bitStream.Read(hasPlatformData3);
		ASSERT_TRUE(hasPlatformData3);

		eMoverSubComponentType platformType3;
		bitStream.Read(platformType3);
		ASSERT_EQ(platformType3, eMoverSubComponentType::SimpleMover);

		bool isDirty3;
		bitStream.Read(isDirty3);
		ASSERT_TRUE(isDirty3);

		eMovementPlatformState state3;
		bitStream.Read(state3);
		ASSERT_EQ(state3, eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint);

		int32_t desiredWaypointIndex3;
		bitStream.Read(desiredWaypointIndex3);
		ASSERT_EQ(desiredWaypointIndex3, 23);

		bool shouldStopAtDesiredWaypoint3;
		bitStream.Read(shouldStopAtDesiredWaypoint3);
		ASSERT_TRUE(shouldStopAtDesiredWaypoint3);

		bool isInReverse4;
		bitStream.Read(isInReverse4);
		ASSERT_TRUE(isInReverse4);

		float percentBetweenPoints3;
		bitStream.Read(percentBetweenPoints3);
		ASSERT_EQ(percentBetweenPoints3, 24);

		NiPoint3 position3;
		bitStream.Read(position3.x);
		bitStream.Read(position3.y);
		bitStream.Read(position3.z);
		ASSERT_EQ(position3, NiPoint3(25, 26, 27));

		uint32_t currentWaypointIndex3;
		bitStream.Read(currentWaypointIndex3);
		ASSERT_EQ(currentWaypointIndex3, 28);

		uint32_t nextWaypointIndex3;
		bitStream.Read(nextWaypointIndex3);
		ASSERT_EQ(nextWaypointIndex3, 29);

		float idleTimeElapsed3;
		bitStream.Read(idleTimeElapsed3);
		ASSERT_EQ(idleTimeElapsed3, 30);

		float moveTimeElapsed3;
		bitStream.Read(moveTimeElapsed3);
		ASSERT_EQ(moveTimeElapsed3, 33);

		bool hasPlatformSubComponents2;
		bitStream.Read(hasPlatformSubComponents2);
		ASSERT_FALSE(hasPlatformSubComponents2);
	}
};

TEST_F(MovingPlatformComponentTests, MovingPlatformConstructionTest) {
	TestSerialization();
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSerializationTest) {
	TestSerialization();
}
