#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "QuickBuildComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class QuickBuildComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	QuickBuildComponent* quickBuildComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		quickBuildComponent = baseEntity->AddComponent<QuickBuildComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(QuickBuildComponentTest, QuickBuildComponentSerializeInitialTest) {
	quickBuildComponent->Serialize(bitStream, true);
	
	// QuickBuild without Destroyable component should write specific pattern
	bool hasDestroyableFlag1, hasDestroyableFlag2, hasDestroyableFlag3;
	bitStream.Read(hasDestroyableFlag1);
	bitStream.Read(hasDestroyableFlag2);
	bitStream.Read(hasDestroyableFlag3);
	
	ASSERT_FALSE(hasDestroyableFlag1);
	ASSERT_FALSE(hasDestroyableFlag2);
	ASSERT_FALSE(hasDestroyableFlag3);
	
	// Should write scripted activity flag
	bool hasScriptedActivity;
	bitStream.Read(hasScriptedActivity);
	ASSERT_TRUE(hasScriptedActivity);
	
	// Should write builder count (should be 0 initially)
	uint32_t builderCount;
	bitStream.Read(builderCount);
	ASSERT_EQ(builderCount, 0);
}

TEST_F(QuickBuildComponentTest, QuickBuildComponentSerializeUpdateTest) {
	quickBuildComponent->Serialize(bitStream, false);
	
	// Non-initial update should still write some flags
	bool hasDestroyableFlag1, hasDestroyableFlag2;
	bitStream.Read(hasDestroyableFlag1);
	bitStream.Read(hasDestroyableFlag2);
	
	ASSERT_FALSE(hasDestroyableFlag1);
	ASSERT_FALSE(hasDestroyableFlag2);
	
	// Should write scripted activity flag
	bool hasScriptedActivity;
	bitStream.Read(hasScriptedActivity);
	ASSERT_TRUE(hasScriptedActivity);
	
	// Should write builder count
	uint32_t builderCount;
	bitStream.Read(builderCount);
	ASSERT_EQ(builderCount, 0);
}



TEST_F(QuickBuildComponentTest, QuickBuildComponentResetTimeTest) {
	// Test reset time functionality
	quickBuildComponent->SetResetTime(30.0f);
	ASSERT_EQ(quickBuildComponent->GetResetTime(), 30.0f);
	
	quickBuildComponent->SetResetTime(0.0f);
	ASSERT_EQ(quickBuildComponent->GetResetTime(), 0.0f);
}

TEST_F(QuickBuildComponentTest, QuickBuildComponentCompleteTimeTest) {
	// Test complete time functionality
	quickBuildComponent->SetCompleteTime(15.0f);
	ASSERT_EQ(quickBuildComponent->GetCompleteTime(), 15.0f);
}