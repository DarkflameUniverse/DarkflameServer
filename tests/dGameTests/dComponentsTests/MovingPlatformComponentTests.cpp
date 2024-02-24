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
	Path pathOnce;
	Path pathBounce;
	Path pathLoop;
	void SetUp() override {
		SetUpDependencies();
		PathWaypoint waypointStart;
		waypointStart.position = NiPoint3(1, 2, 3);
		waypointStart.rotation = NiQuaternion(4, 5, 6, 7);
		waypointStart.movingPlatform.speed = 16.0f;

		PathWaypoint waypointMiddle;
		waypointMiddle.position = NiPoint3(4, 5, 6);
		waypointMiddle.rotation = NiQuaternion(7, 8, 9, 10);
		waypointStart.movingPlatform.speed = 16.0f;

		PathWaypoint waypointEnd;
		waypointEnd.position = NiPoint3(4, 5, 7);
		waypointEnd.rotation = NiQuaternion(7, 8, 9, 10);
		waypointStart.movingPlatform.speed = 16.0f;
		{
			pathOnce.movingPlatform.timeBasedMovement = false;
			pathOnce.pathBehavior = PathBehavior::Once;
			pathOnce.pathName = "ExampleOncePath";

			pathOnce.pathWaypoints.push_back(waypointStart);
			pathOnce.pathWaypoints.push_back(waypointMiddle);
			pathOnce.pathWaypoints.push_back(waypointEnd);

			Game::zoneManager->GetZone()->AddPath(pathOnce);
		}
		{
			pathBounce.movingPlatform.timeBasedMovement = false;
			pathBounce.pathBehavior = PathBehavior::Bounce;
			pathBounce.pathName = "ExampleBouncePath";

			pathBounce.pathWaypoints.push_back(waypointStart);
			pathBounce.pathWaypoints.push_back(waypointMiddle);
			pathBounce.pathWaypoints.push_back(waypointEnd);

			Game::zoneManager->GetZone()->AddPath(pathBounce);
		}
		{
			pathLoop.movingPlatform.timeBasedMovement = false;
			pathLoop.pathBehavior = PathBehavior::Loop;
			pathLoop.pathName = "ExampleLoopPath";


			pathLoop.pathWaypoints.push_back(waypointStart);
			pathLoop.pathWaypoints.push_back(waypointMiddle);
			pathLoop.pathWaypoints.push_back(waypointEnd);

			Game::zoneManager->GetZone()->AddPath(pathLoop);
		}
		// Set our starting position
		info.pos = NiPoint3(25, 26, 27);
		info.rot = NiQuaternion(28, 29, 30, 31);

		// Simple mover data
		info.settings.push_back(new LDFData<float>(u"platformMoveX", 23));
		info.settings.push_back(new LDFData<float>(u"platformMoveY", 453));
		info.settings.push_back(new LDFData<float>(u"platformMoveZ", 523));
		info.settings.push_back(new LDFData<float>(u"platformMoveTime", 5724));
		info.settings.push_back(new LDFData<bool>(u"platformStartAtEnd", true));
		info.settings.push_back(new LDFData<bool>(u"dbonly", false));
		info.settings.push_back(new LDFData<bool>(u"platformIsMover", true));
		info.settings.push_back(new LDFData<bool>(u"platformIsSimpleMover", true));
		info.settings.push_back(new LDFData<bool>(u"platformIsRotater", true));

		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);

		auto* simplePhysicsComponent = baseEntity->AddComponent<SimplePhysicsComponent>(1);
		auto* movingPlatformComponent = baseEntity->AddComponent<MovingPlatformComponent>("ExampleOncePath");
		movingPlatformComponent->LoadConfigData();
		movingPlatformComponent->LoadDataFromTemplate();
	}

	void TearDown() override {
		TearDownDependencies();
	}

	void DeserializeSimpleMoverPlatformSubComponent() {
		bool dirtyStartingPoint = false;
		bitStream.Read(dirtyStartingPoint);
		ASSERT_TRUE(dirtyStartingPoint);

		bool hasStartingPoint = false;
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

		bool isDirty;
		bitStream.Read(isDirty);
		ASSERT_TRUE(isDirty);

		eMovementPlatformState state;
		bitStream.Read(state);
		ASSERT_EQ(state, eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint);
	
		int32_t currentWaypointIndex;
		bitStream.Read(currentWaypointIndex);
		ASSERT_EQ(currentWaypointIndex, 0);

		bool isInReverse;
		bitStream.Read(isInReverse);
		ASSERT_FALSE(isInReverse);
	}

	void DeserializeMovingPlatformSubComponent() {
		bool isDirty;
		ASSERT_TRUE(bitStream.Read(isDirty));
		ASSERT_TRUE(isDirty);

		eMovementPlatformState state;
		ASSERT_TRUE(bitStream.Read(state));
		ASSERT_EQ(state, eMovementPlatformState::Stopped | eMovementPlatformState::ReachedDesiredWaypoint);

		int32_t desiredWaypointIndex;
		ASSERT_TRUE(bitStream.Read(desiredWaypointIndex));
		ASSERT_EQ(desiredWaypointIndex, -1);

		bool shouldStopAtDesiredWaypoint;
		ASSERT_TRUE(bitStream.Read(shouldStopAtDesiredWaypoint));
		ASSERT_FALSE(shouldStopAtDesiredWaypoint);

		bool isInReverse2;
		ASSERT_TRUE(bitStream.Read(isInReverse2));
		ASSERT_FALSE(isInReverse2);

		float percentBetweenPoints;
		ASSERT_TRUE(bitStream.Read(percentBetweenPoints));
		ASSERT_EQ(percentBetweenPoints, 0);

		NiPoint3 position;
		ASSERT_TRUE(bitStream.Read(position.x));
		ASSERT_TRUE(bitStream.Read(position.y));
		ASSERT_TRUE(bitStream.Read(position.z));
		ASSERT_EQ(position, NiPoint3(25, 26, 27));

		uint32_t currentWaypointIndex;
		ASSERT_TRUE(bitStream.Read(currentWaypointIndex));
		ASSERT_EQ(currentWaypointIndex, 0);

		uint32_t nextWaypointIndex;
		ASSERT_TRUE(bitStream.Read(nextWaypointIndex));
		ASSERT_EQ(nextWaypointIndex, -1);

		float idleTimeElapsed;
		ASSERT_TRUE(bitStream.Read(idleTimeElapsed));
		ASSERT_FLOAT_EQ(idleTimeElapsed, 0.0f);

		float moveTimeElapsed;
		ASSERT_TRUE(bitStream.Read(moveTimeElapsed));
		ASSERT_FLOAT_EQ(moveTimeElapsed, 0.0f);
	}

	void DeserializeMovingPlatformComponent() {
		// read in the full BitStream and check the values match what they were set to above
		bool hasPlatformSubComponents = false;
		ASSERT_TRUE(bitStream.Read(hasPlatformSubComponents));
		ASSERT_TRUE(hasPlatformSubComponents);

		bool dirtyPathInfo;
		ASSERT_TRUE(bitStream.Read(dirtyPathInfo));
		ASSERT_TRUE(dirtyPathInfo);

		bool hasPath;
		ASSERT_TRUE(bitStream.Read(hasPath));
		ASSERT_TRUE(hasPath);

		std::u16string pathName;
		uint16_t pathNameLength;
		ASSERT_TRUE(bitStream.Read(pathNameLength));
		pathName.resize(pathNameLength);
		ASSERT_TRUE(bitStream.ReadBits(reinterpret_cast<unsigned char*>(pathName.data()), BYTES_TO_BITS(pathNameLength) * 2));
		ASSERT_EQ(pathName, u"ExampleOncePath");

		uint32_t pathStartIndex;
		ASSERT_TRUE(bitStream.Read(pathStartIndex));
		ASSERT_EQ(pathStartIndex, 0);

		bool isInReverse;
		ASSERT_TRUE(bitStream.Read(isInReverse));
		ASSERT_FALSE(isInReverse);

		bool hasPlatformData;
		ASSERT_TRUE(bitStream.Read(hasPlatformData));
		ASSERT_TRUE(hasPlatformData);

		eMoverSubComponentType platformType;
		ASSERT_TRUE(bitStream.Read(platformType));
		ASSERT_EQ(platformType, eMoverSubComponentType::Mover);
		DeserializeMovingPlatformSubComponent();

		ASSERT_TRUE(bitStream.Read(hasPlatformData));
		ASSERT_TRUE(hasPlatformData);

		ASSERT_TRUE(bitStream.Read(platformType));
		ASSERT_EQ(platformType, eMoverSubComponentType::SimpleMover);
		DeserializeSimpleMoverPlatformSubComponent();

		ASSERT_TRUE(bitStream.Read(hasPlatformData));
		ASSERT_TRUE(hasPlatformData);

		ASSERT_TRUE(bitStream.Read(platformType));
		ASSERT_EQ(platformType, eMoverSubComponentType::Rotator);
		DeserializeMovingPlatformSubComponent();

		ASSERT_TRUE(bitStream.Read(hasPlatformData));
		ASSERT_FALSE(hasPlatformData);
	}

	void TestSerialization() {
		auto* movingPlatformComponent = baseEntity->GetComponent<MovingPlatformComponent>();
		ASSERT_NE(movingPlatformComponent, nullptr);
		movingPlatformComponent->Serialize(&bitStream, true);
		DeserializeMovingPlatformComponent();
	}
};

TEST_F(MovingPlatformComponentTests, MovingPlatformConstructionTest) {
	TestSerialization();
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSerializationTest) {
	TestSerialization();
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentPathOnceAdvanceTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExampleOncePath", 0, false);
	
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 1);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 2);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 2);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentPathBounceAdvanceTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExampleBouncePath", 0, false);
	
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 1);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 2);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 1);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 0);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentLoopAdvanceTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExampleLoopPath", 0, false);
	
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 1);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 2);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 0);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	ASSERT_FALSE(moverPlatformSubComponent.GetInReverse());
}

TEST_F(MovingPlatformComponentTests, MovingPlatformSubComponentLoopAdvanceReverseTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExampleLoopPath", 0, true);
	
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 2);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 1);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 1);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 0);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
	moverPlatformSubComponent.AdvanceToNextReverseWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypointIndex(), 0);
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypointIndex(), 2);
	ASSERT_TRUE(moverPlatformSubComponent.GetInReverse());
}


TEST_F(MovingPlatformComponentTests, MovingPlatformMoverSpeedCalculationTest) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());

	moverPlatformSubComponent.SetupPath("ExampleOncePath", 0, false);

	ASSERT_EQ(moverPlatformSubComponent.CalculateSpeed(), 16.0f);
	NiPoint3 r = moverPlatformSubComponent.CalculateLinearVelocity();
	ASSERT_FLOAT_EQ(r.x, 9.2376051);
	ASSERT_FLOAT_EQ(r.y, 9.2376051);
	ASSERT_FLOAT_EQ(r.z, 9.2376051);
	moverPlatformSubComponent.AdvanceToNextWaypoint();
}

TEST_F(MovingPlatformComponentTests, MovingPlatformNextAndCurrentWaypointAccess) {
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExampleOncePath", 0, false);

	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypoint().position, NiPoint3(1, 2, 3));
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypoint().position, NiPoint3(4, 5, 6));
	moverPlatformSubComponent.AdvanceToNextWaypoint();
	ASSERT_EQ(moverPlatformSubComponent.GetCurrentWaypoint().position, NiPoint3(4, 5, 6));
	ASSERT_EQ(moverPlatformSubComponent.GetNextWaypoint().position, NiPoint3(4, 5, 7));
}

TEST_F(MovingPlatformComponentTests, MovingPlatformRunTest) {
	baseEntity->SetPosition(NiPoint3(99.296440f, 419.293335f, 207.219498f));
	Path path;

	PathWaypoint waypointStart;
	waypointStart.position = NiPoint3(99.296440f, 419.293335f, 207.219498f);
	waypointStart.rotation = NiQuaternion(0, 0, 0, 0);
	waypointStart.movingPlatform.speed = 16.0f;

	PathWaypoint waypointEnd;
	waypointEnd.position = NiPoint3(141.680099f, 419.990051f, 208.680450f);
	waypointEnd.rotation = NiQuaternion(0, 0, 0, 0);

	waypointStart.movingPlatform.speed = 16.0f;
	path.movingPlatform.timeBasedMovement = false;
	path.pathBehavior = PathBehavior::Once;
	path.pathName = "ExamplePath";

	path.pathWaypoints.push_back(waypointStart);
	path.pathWaypoints.push_back(waypointEnd);

	Game::zoneManager->GetZone()->AddPath(path);
	MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
	moverPlatformSubComponent.SetupPath("ExamplePath", 0, false);

	moverPlatformSubComponent.UpdateLinearVelocity();
	moverPlatformSubComponent.StartPathing();
	moverPlatformSubComponent.Update(2.65f);

	auto [x,y,z] = moverPlatformSubComponent.GetPosition();
	// just check that its close enough
	EXPECT_LT(141.680099f - x, 0.1f);
	EXPECT_LT(419.990051f - y, 0.1f);
	EXPECT_LT(208.680450f - z, 0.1f);
}

TEST_F(MovingPlatformComponentTests, MovingPlatformPercentBetweenPointsTest) {
	
	Path path;

	PathWaypoint waypointStart;
	waypointStart.position = NiPoint3(0, 0, 0);
	waypointStart.rotation = NiQuaternion(0, 0, 0, 0);
	waypointStart.movingPlatform.speed = 16.0f;

	PathWaypoint waypointEnd;
	waypointEnd.position = NiPoint3(2, 0, 0);
	waypointEnd.rotation = NiQuaternion(0, 0, 0, 0);

	waypointStart.movingPlatform.speed = 16.0f;
	path.movingPlatform.timeBasedMovement = false;
	path.pathBehavior = PathBehavior::Once;
	path.pathName = "ExamplePath";

	path.pathWaypoints.push_back(waypointStart);
	path.pathWaypoints.push_back(waypointEnd);

	Game::zoneManager->GetZone()->AddPath(path);
	{
		baseEntity->SetPosition(NiPoint3(0, 0, 0));
		MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
		moverPlatformSubComponent.SetupPath("ExamplePath", 0, false);
		EXPECT_FLOAT_EQ(moverPlatformSubComponent.CalculatePercentToNextWaypoint(), 0.0f);
	}
	{
		baseEntity->SetPosition(NiPoint3(1, 0, 0));
		MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
		moverPlatformSubComponent.SetupPath("ExamplePath", 0, false);
		EXPECT_FLOAT_EQ(moverPlatformSubComponent.CalculatePercentToNextWaypoint(), 0.5f);
	}
	{
		baseEntity->SetPosition(NiPoint3(2, 0, 0));
		MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
		moverPlatformSubComponent.SetupPath("ExamplePath", 0, false);
		EXPECT_FLOAT_EQ(moverPlatformSubComponent.CalculatePercentToNextWaypoint(), 1.0f);
	}
	{
		path.movingPlatform.timeBasedMovement = true;
		path.pathName = "ExamplePath2";
		Game::zoneManager->GetZone()->AddPath(path);
		baseEntity->SetPosition(NiPoint3(1, 0, 0));
		MoverPlatformSubComponent moverPlatformSubComponent(baseEntity->GetComponent<MovingPlatformComponent>());
		moverPlatformSubComponent.SetupPath("ExamplePath2", 0, false);
		ASSERT_FLOAT_EQ(moverPlatformSubComponent.CalculatePercentToNextWaypoint(), 0.0f);
	}
}
