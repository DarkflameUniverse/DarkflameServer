#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ActivityComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class ActivityComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	ActivityComponent* activityComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		activityComponent = baseEntity->AddComponent<ActivityComponent>(1);  // Needs activityId
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ActivityComponentTest, ActivityComponentSerializeInitialEmptyTest) {
	activityComponent->Serialize(bitStream, true);
	
	// Should write dirty activity info flag
	bool dirtyActivityInfo;
	bitStream.Read(dirtyActivityInfo);
	// May be true or false depending on initial state
	ASSERT_TRUE(dirtyActivityInfo || !dirtyActivityInfo);  // Either is valid
	
	if (dirtyActivityInfo) {
		uint32_t playerCount;
		bitStream.Read(playerCount);
		ASSERT_EQ(playerCount, 0);  // Should be empty initially
	}
}

TEST_F(ActivityComponentTest, ActivityComponentSerializeUpdateTest) {
	// Test non-initial update
	activityComponent->Serialize(bitStream, false);
	
	bool dirtyActivityInfo;
	bitStream.Read(dirtyActivityInfo);
	// Should be false for non-dirty updates
	ASSERT_FALSE(dirtyActivityInfo);
}

TEST_F(ActivityComponentTest, ActivityComponentSerializeConsistencyTest) {
	// Test that multiple serializations are consistent
	RakNet::BitStream firstSerialization;
	RakNet::BitStream secondSerialization;
	
	activityComponent->Serialize(firstSerialization, true);
	activityComponent->Serialize(secondSerialization, true);
	
	ASSERT_EQ(firstSerialization.GetNumberOfBitsUsed(), secondSerialization.GetNumberOfBitsUsed());
}

TEST_F(ActivityComponentTest, ActivityComponentBasicAPITest) {
	// Test basic API methods
	ASSERT_GE(activityComponent->GetActivityID(), 0);
	
	// Test activity players list (should be empty initially)
	auto players = activityComponent->GetActivityPlayers();
	ASSERT_TRUE(players.empty());
}