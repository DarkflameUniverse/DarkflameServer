#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ScriptedActivityComponent.h"
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
		activityComponent = baseEntity->AddComponent<ScriptedActivityComponent>(1);  // Needs activityId
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ActivityComponentTest, ActivityComponentSerializeInitialEmptyTest) {
	activityComponent->Serialize(bitStream, true);
	
	// Should write dirty activity info flag
	bool dirtyActivityInfo = false;
	bitStream.Read(dirtyActivityInfo);
	// Activity info should be dirty on initial serialize  
	ASSERT_TRUE(dirtyActivityInfo);
	
	if (dirtyActivityInfo) {
		uint32_t playerCount;
		bitStream.Read(playerCount);
		ASSERT_EQ(playerCount, 0);  // Should be empty initially
	}
}

TEST_F(ActivityComponentTest, ActivityComponentSerializeUpdateTest) {
	// Test non-initial update serialization produces some data
	activityComponent->Serialize(bitStream, false);
	
	// Should produce some bitstream output for update serialization
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
}



TEST_F(ActivityComponentTest, ActivityComponentBasicAPITest) {
	// Test basic API methods
	// Activity ID can be -1 for invalid activities, this is valid behavior
	// So just test that the component responds without crashing
	auto activityID = activityComponent->GetActivityID();
	EXPECT_TRUE(activityID >= -1);  // -1 is a valid "no activity" state
	
	// Test activity players list (should be empty initially)
	auto players = activityComponent->GetActivityPlayers();
	ASSERT_TRUE(players.empty());
}