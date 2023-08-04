#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"

#define _MOVING_PLATFORM_TEST
#include "MovingPlatformComponent.h"
#undef _MOVING_PLATFORM_TEST
#include "Zone.h"
#include "SimplePhysicsComponent.h"
#include "eReplicaComponentType.h"

class MovingPlatformComponentTests : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	CBITSTREAM;
	uint32_t flags = 0;
	Path path;
	void SetUp() override {
		SetUpDependencies();
		path.movingPlatform.timeBasedMovement = false;
		path.pathBehavior = PathBehavior::Once;
		PathWaypoint waypointStart;
		waypointStart.position = NiPoint3(1, 2, 3);
		waypointStart.rotation = NiQuaternion(4, 5, 6, 7);

		PathWaypoint waypointMiddle;
		waypointMiddle.position = NiPoint3(4, 5, 6);
		waypointMiddle.rotation = NiQuaternion(7, 8, 9, 10);

		PathWaypoint waypointEnd;
		waypointEnd.position = NiPoint3(4, 5, 6);
		waypointEnd.rotation = NiQuaternion(7, 8, 9, 10);

		path.pathWaypoints.push_back(waypointStart);
		path.pathWaypoints.push_back(waypointMiddle);
		path.pathWaypoints.push_back(waypointEnd);

		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		baseEntity->SetVar<bool>(u"dbonly", false);
		baseEntity->SetVar<float>(u"platformMoveX", 23);
		baseEntity->SetVar<float>(u"platformMoveY", 453);
		baseEntity->SetVar<float>(u"platformMoveZ", 523);
		baseEntity->SetVar<float>(u"platformMoveTime", 5724);
		baseEntity->SetVar<bool>(u"platformStartAtEnd", true);
		auto* movingPlatformComponent = new MovingPlatformComponent(baseEntity.get(), "");
		auto* simplePhysicsComponent = new SimplePhysicsComponent(1, baseEntity.get());
		baseEntity->AddComponent(eReplicaComponentType::MOVING_PLATFORM, movingPlatformComponent);
		baseEntity->AddComponent(eReplicaComponentType::SIMPLE_PHYSICS, simplePhysicsComponent);
		baseEntity->SetPosition(NiPoint3(25, 26, 27));
		baseEntity->SetRotation(NiQuaternion(28, 29, 30, 31));
		auto moverPlatformSubComponent = std::make_unique<MoverPlatformSubComponent>(movingPlatformComponent);
		moverPlatformSubComponent->m_State = eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint;
		moverPlatformSubComponent->m_DesiredWaypointIndex = 1;
		moverPlatformSubComponent->m_PercentUntilNextWaypoint = 2;
		moverPlatformSubComponent->m_Position = NiPoint3(3, 4, 5);
		moverPlatformSubComponent->m_CurrentWaypointIndex = 6;
		moverPlatformSubComponent->m_NextWaypointIndex = 7;
		moverPlatformSubComponent->m_IdleTimeElapsed = 8;
		moverPlatformSubComponent->m_MoveTimeElapsed = 11;
		moverPlatformSubComponent->m_IsDirty = true;
		moverPlatformSubComponent->m_InReverse = true;
		moverPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;

		auto rotatorPlatformSubComponent = std::make_unique<RotatorPlatformSubComponent>(movingPlatformComponent);
		rotatorPlatformSubComponent->m_State = eMovementPlatformState::Travelling;
		rotatorPlatformSubComponent->m_DesiredWaypointIndex = 12;
		rotatorPlatformSubComponent->m_PercentUntilNextWaypoint = 13;
		rotatorPlatformSubComponent->m_Position = NiPoint3(14, 15, 16);
		rotatorPlatformSubComponent->m_CurrentWaypointIndex = 17;
		rotatorPlatformSubComponent->m_NextWaypointIndex = 18;
		rotatorPlatformSubComponent->m_IdleTimeElapsed = 19;
		rotatorPlatformSubComponent->m_MoveTimeElapsed = 22;
		rotatorPlatformSubComponent->m_IsDirty = true;
		rotatorPlatformSubComponent->m_InReverse = true;
		rotatorPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;

		auto simpleMoverPlatformSubComponent = std::make_unique<SimpleMoverPlatformSubComponent>(movingPlatformComponent, NiPoint3(), true);
		simpleMoverPlatformSubComponent->m_State = eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint;
		simpleMoverPlatformSubComponent->m_DesiredWaypointIndex = 23;
		simpleMoverPlatformSubComponent->m_PercentUntilNextWaypoint = 24;
		simpleMoverPlatformSubComponent->m_CurrentWaypointIndex = 28;
		simpleMoverPlatformSubComponent->m_NextWaypointIndex = 29;
		simpleMoverPlatformSubComponent->m_IdleTimeElapsed = 30;
		simpleMoverPlatformSubComponent->m_MoveTimeElapsed = 33;
		simpleMoverPlatformSubComponent->m_IsDirty = true;
		simpleMoverPlatformSubComponent->m_InReverse = true;
		simpleMoverPlatformSubComponent->m_DirtyStartingPoint = true;
		simpleMoverPlatformSubComponent->m_HasStartingPoint = true;
		simpleMoverPlatformSubComponent->m_ShouldStopAtDesiredWaypoint = true;
		simpleMoverPlatformSubComponent->LoadConfigData();

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
		ASSERT_EQ(pathStartIndex, 0);

		bool isInReverse;
		bitStream.Read(isInReverse);
		ASSERT_FALSE(isInReverse);

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

		bool dirtyStartingPoint;
		bitStream.Read(dirtyStartingPoint);
		ASSERT_TRUE(dirtyStartingPoint);

		bool hasStartingPoint;
		bitStream.Read(hasStartingPoint);
		ASSERT_TRUE(hasStartingPoint);

		NiPoint3 startingPoint;
		bitStream.Read(startingPoint.x);
		bitStream.Read(startingPoint.y);
		bitStream.Read(startingPoint.z);
		ASSERT_EQ(startingPoint, NiPoint3(25, 26, 27));

		NiQuaternion startingRotation;
		bitStream.Read(startingRotation.w);
		bitStream.Read(startingRotation.x);
		bitStream.Read(startingRotation.y);
		bitStream.Read(startingRotation.z);
		ASSERT_EQ(startingRotation, NiQuaternion(28, 29, 30, 31));

		bool isDirty4;
		bitStream.Read(isDirty4);
		ASSERT_TRUE(isDirty4);

		eMovementPlatformState state3;
		bitStream.Read(state3);
		ASSERT_EQ(state3, eMovementPlatformState::Waiting | eMovementPlatformState::ReachedDesiredWaypoint | eMovementPlatformState::ReachedFinalWaypoint);

		int32_t currentWaypointIndex3;
		bitStream.Read(currentWaypointIndex3);
		ASSERT_EQ(currentWaypointIndex3, 28);

		bool isInReverse4;
		bitStream.Read(isInReverse4);
		ASSERT_TRUE(isInReverse4);

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

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentPathAdvanceForwardTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(nullptr);
	moverPlatformSubComponent._SetPath(&path);
	moverPlatformSubComponent.m_CurrentWaypointIndex = 0;
	moverPlatformSubComponent.m_NextWaypointIndex = 1;
	moverPlatformSubComponent.m_InReverse = false;
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 1);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 2);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 2);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 2);
	ASSERT_FALSE(moverPlatformSubComponent.m_InReverse);
	path.pathBehavior = PathBehavior::Bounce;
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 2);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 1);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentPathAdvanceReverseTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(nullptr);
	moverPlatformSubComponent._SetPath(&path);
	moverPlatformSubComponent.m_CurrentWaypointIndex = 2;
	moverPlatformSubComponent.m_NextWaypointIndex = 1;
	moverPlatformSubComponent.m_InReverse = true;
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 1);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 0);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 0);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 0);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
	path.pathBehavior = PathBehavior::Bounce;
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 0);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 1);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentPathAdvanceTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(nullptr);
	moverPlatformSubComponent._SetPath(&path);
	path.pathBehavior = PathBehavior::Bounce;
	moverPlatformSubComponent.m_CurrentWaypointIndex = 0;
	moverPlatformSubComponent.m_NextWaypointIndex = 1;
	moverPlatformSubComponent.m_InReverse = false;
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 1);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 2);
	ASSERT_FALSE(moverPlatformSubComponent.m_InReverse);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 2);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 1);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 1);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 0);
	ASSERT_TRUE(moverPlatformSubComponent.m_InReverse);
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 0);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 1);
	ASSERT_FALSE(moverPlatformSubComponent.m_InReverse);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.m_CurrentWaypointIndex, 1);
	ASSERT_EQ(moverPlatformSubComponent.m_NextWaypointIndex, 2);
	ASSERT_FALSE(moverPlatformSubComponent.m_InReverse);
}

TEST_F(MovingPlatformComponentTests, MovingPlatformMoverSpeedCalculationTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(nullptr);
	path.pathWaypoints.at(0).position = NiPoint3(99.296440, 419.293335, 207.219498);
	path.pathWaypoints.at(0).movingPlatform.speed = 16.0f;
	
	path.pathWaypoints.at(1).position = NiPoint3(141.680099, 419.990051, 208.680450);
	path.pathWaypoints.at(1).movingPlatform.speed = 16.0f;
	path.pathBehavior = PathBehavior::Bounce;
	moverPlatformSubComponent._SetPath(&path);
	moverPlatformSubComponent.m_Speed = 16.0f;
	moverPlatformSubComponent.m_TimeBasedMovement = false;
	moverPlatformSubComponent.m_InReverse = false;
	moverPlatformSubComponent.m_CurrentWaypointIndex = 0;
	moverPlatformSubComponent.m_NextWaypointIndex = 1;
	ASSERT_EQ(moverPlatformSubComponent.CalculateSpeed(), 16.0f);
	NiPoint3 r = moverPlatformSubComponent.CalculateLinearVelocity();
	ASSERT_FLOAT_EQ(r.x, 15.988346099854);
	ASSERT_FLOAT_EQ(r.y, 0.26282161474228);
	ASSERT_FLOAT_EQ(r.z, 0.5511137843132);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
}
