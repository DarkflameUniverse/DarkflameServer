#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "MovingPlatformComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eMovementPlatformState.h"

class MovingPlatformTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	MovingPlatformComponent* movingPlatformComponent;
	CBITSTREAM
		uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		movingPlatformComponent = baseEntity->AddComponent<MovingPlatformComponent>("testPath");
		// Initialize some values to be not default
		movingPlatformComponent->SetSerialized(true);
		
		// Set up the MoverSubComponent with some test values
		auto* moverSubComponent = movingPlatformComponent->GetMoverSubComponent();
		if (moverSubComponent) {
			moverSubComponent->mState = eMovementPlatformState::Moving;
			moverSubComponent->mDesiredWaypointIndex = 5;
			moverSubComponent->mShouldStopAtDesiredWaypoint = true;
			moverSubComponent->mInReverse = false;
			moverSubComponent->mPercentBetweenPoints = 0.75f;
			moverSubComponent->mPosition = NiPoint3(10.0f, 20.0f, 30.0f);
			moverSubComponent->mCurrentWaypointIndex = 3;
			moverSubComponent->mNextWaypointIndex = 4;
			moverSubComponent->mIdleTimeElapsed = 2.5f;
		}
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of a MovingPlatformComponent with m_Serialize = false
 */
TEST_F(MovingPlatformTest, MovingPlatformComponentSerializeDisabledTest) {
	bitStream.Reset();
	
	// Set m_Serialize to false to test the early return path
	movingPlatformComponent->SetSerialized(false);
	
	// Now we test a serialization for correctness.
	movingPlatformComponent->Serialize(bitStream, false);
	
	// Should only write two false booleans
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 2);
	
	bool firstFlag;
	bool secondFlag;
	bitStream.Read(firstFlag);
	bitStream.Read(secondFlag);
	
	EXPECT_EQ(firstFlag, false);
	EXPECT_EQ(secondFlag, false);
}

/**
 * Test serialization of a MovingPlatformComponent with enabled serialization but no path
 */
TEST_F(MovingPlatformTest, MovingPlatformComponentSerializeNoPathTest) {
	bitStream.Reset();
	
	// Create a component with no path to test the path logic
	auto* entityNoPath = new Entity(16, GameDependenciesTest::info);
	auto* componentNoPath = entityNoPath->AddComponent<MovingPlatformComponent>("");
	componentNoPath->SetSerialized(true);
	
	// Stop pathing to make hasPath false
	componentNoPath->StopPathing();
	
	componentNoPath->Serialize(bitStream, false);
	
	// Should write: true (m_Serialize), false (hasPath), true (hasPlatform), mover type, then mover data
	bool isEnabled;
	bool hasPath;
	bool hasPlatform;
	
	bitStream.Read(isEnabled);
	bitStream.Read(hasPath);
	bitStream.Read(hasPlatform);
	
	EXPECT_EQ(isEnabled, true);
	EXPECT_EQ(hasPath, false);
	EXPECT_EQ(hasPlatform, true);
	
	// Should continue with platform serialization
	eMoverSubComponentType moverType;
	bitStream.Read(moverType);
	EXPECT_EQ(moverType, eMoverSubComponentType::mover);
	
	// Clean up
	delete entityNoPath;
}

/**
 * Test complete serialization of a MovingPlatformComponent with path
 */
TEST_F(MovingPlatformTest, MovingPlatformComponentSerializeFullTest) {
	bitStream.Reset();
	
	// Now we test a serialization for correctness.
	movingPlatformComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isEnabled;
	bool hasPath;
	
	bitStream.Read(isEnabled);
	bitStream.Read(hasPath);
	
	EXPECT_EQ(isEnabled, true);
	EXPECT_EQ(hasPath, true);
	
	if (hasPath) {
		bool isOnRail;
		bitStream.Read(isOnRail);
		EXPECT_EQ(isOnRail, true);
		
		uint16_t pathNameSize;
		bitStream.Read(pathNameSize);
		EXPECT_EQ(pathNameSize, 8); // "testPath" length
		
		std::u16string pathName;
		for (uint16_t i = 0; i < pathNameSize; i++) {
			uint16_t character;
			bitStream.Read(character);
			pathName += character;
		}
		
		uint32_t startingPoint;
		bitStream.Read(startingPoint);
		EXPECT_EQ(startingPoint, 0);
		
		bool reverse;
		bitStream.Read(reverse);
		EXPECT_EQ(reverse, false);
	}
	
	bool hasPlatform;
	bitStream.Read(hasPlatform);
	EXPECT_EQ(hasPlatform, true);
	
	if (hasPlatform) {
		eMoverSubComponentType moverType;
		bitStream.Read(moverType);
		EXPECT_EQ(moverType, eMoverSubComponentType::mover);
		
		// Test MoverSubComponent serialization
		bool moverHasData;
		bitStream.Read(moverHasData);
		EXPECT_EQ(moverHasData, true);
		
		eMovementPlatformState state;
		bitStream.Read(state);
		EXPECT_EQ(state, eMovementPlatformState::Moving);
		
		int32_t desiredWaypointIndex;
		bitStream.Read(desiredWaypointIndex);
		EXPECT_EQ(desiredWaypointIndex, 5);
		
		bool shouldStopAtDesiredWaypoint;
		bitStream.Read(shouldStopAtDesiredWaypoint);
		EXPECT_EQ(shouldStopAtDesiredWaypoint, true);
		
		bool inReverse;
		bitStream.Read(inReverse);
		EXPECT_EQ(inReverse, false);
		
		float percentBetweenPoints;
		bitStream.Read(percentBetweenPoints);
		EXPECT_EQ(percentBetweenPoints, 0.75f);
		
		float positionX, positionY, positionZ;
		bitStream.Read(positionX);
		bitStream.Read(positionY);
		bitStream.Read(positionZ);
		EXPECT_EQ(positionX, 10.0f);
		EXPECT_EQ(positionY, 20.0f);
		EXPECT_EQ(positionZ, 30.0f);
		
		uint32_t currentWaypointIndex;
		bitStream.Read(currentWaypointIndex);
		EXPECT_EQ(currentWaypointIndex, 3);
		
		uint32_t nextWaypointIndex;
		bitStream.Read(nextWaypointIndex);
		EXPECT_EQ(nextWaypointIndex, 4);
		
		float idleTimeElapsed;
		bitStream.Read(idleTimeElapsed);
		EXPECT_EQ(idleTimeElapsed, 2.5f);
		
		float moveTimeElapsed;
		bitStream.Read(moveTimeElapsed);
		EXPECT_EQ(moveTimeElapsed, 0.0f); // Always 0 in current implementation
	}
}

/**
 * Test MoverSubComponent initialization and basic functionality
 */
TEST_F(MovingPlatformTest, MoverSubComponentInitializationTest) {
	auto* moverSubComponent = movingPlatformComponent->GetMoverSubComponent();
	ASSERT_NE(moverSubComponent, nullptr);
	
	// Test that we can access and modify the mover sub component
	moverSubComponent->mState = eMovementPlatformState::Stopped;
	EXPECT_EQ(moverSubComponent->mState, eMovementPlatformState::Stopped);
	
	moverSubComponent->mDesiredWaypointIndex = 10;
	EXPECT_EQ(moverSubComponent->mDesiredWaypointIndex, 10);
}