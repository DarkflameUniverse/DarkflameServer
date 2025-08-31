#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ScriptedActivityComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class ActivityTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	ScriptedActivityComponent* activityComponent;
	CBITSTREAM
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		// ScriptedActivityComponent is the concrete implementation of ActivityComponent
		// that provides the ComponentType required for the Entity template system
		activityComponent = baseEntity->AddComponent<ScriptedActivityComponent>(1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of an ActivityComponent with no players
 */
TEST_F(ActivityTest, ActivityComponentSerializeNoPlayersTest) {
	bitStream.Reset();
	
	// Component should be dirty by default
	// Now we test a serialization for correctness.
	activityComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true);
	
	uint32_t playerCount;
	bitStream.Read(playerCount);
	EXPECT_EQ(playerCount, 0); // No players added
}

/**
 * Test serialization of an ActivityComponent with players
 */
TEST_F(ActivityTest, ActivityComponentSerializeWithPlayersTest) {
	bitStream.Reset();
	
	// Add some test players
	LWOOBJID player1 = 100;
	LWOOBJID player2 = 200;
	
	// Force dirty state for testing by adding and setting values
	activityComponent->SetActivityValue(player1, 0, 10.5f);  // Score
	activityComponent->SetActivityValue(player1, 1, 25.0f);  // Time  
	activityComponent->SetActivityValue(player1, 2, 3.0f);   // Some other metric
	
	activityComponent->SetActivityValue(player2, 0, 15.5f);  // Score
	activityComponent->SetActivityValue(player2, 1, 20.0f);  // Time
	activityComponent->SetActivityValue(player2, 2, 5.0f);   // Some other metric
	
	// Now we test a serialization for correctness.
	activityComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true);
	
	uint32_t playerCount;
	bitStream.Read(playerCount);
	EXPECT_EQ(playerCount, 2);
	
	// Read first player's data
	LWOOBJID readPlayer1;
	bitStream.Read(readPlayer1);
	EXPECT_EQ(readPlayer1, player1);
	
	// Read all 10 values for first player
	for (int i = 0; i < 10; i++) {
		float value;
		bitStream.Read(value);
		float expectedValue = 0.0f; // Default value for most indices
		if (i == 0) expectedValue = 10.5f;
		else if (i == 1) expectedValue = 25.0f;
		else if (i == 2) expectedValue = 3.0f;
		EXPECT_EQ(value, expectedValue);
	}
	
	// Read second player's data  
	LWOOBJID readPlayer2;
	bitStream.Read(readPlayer2);
	EXPECT_EQ(readPlayer2, player2);
	
	// Read all 10 values for second player
	for (int i = 0; i < 10; i++) {
		float value;
		bitStream.Read(value);
		float expectedValue = 0.0f; // Default value for most indices
		if (i == 0) expectedValue = 15.5f;
		else if (i == 1) expectedValue = 20.0f;
		else if (i == 2) expectedValue = 5.0f;
		EXPECT_EQ(value, expectedValue);
	}
}

/**
 * Test serialization of an ActivityComponent during initial update
 */
TEST_F(ActivityTest, ActivityComponentSerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Add a test player and set a value
	LWOOBJID player1 = 100;
	activityComponent->SetActivityValue(player1, 0, 10.5f);
	
	// Now we test a serialization for correctness with initial update.
	activityComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, true);
	
	uint32_t playerCount;
	bitStream.Read(playerCount);
	EXPECT_EQ(playerCount, 1);
	
	// Read player's data
	LWOOBJID readPlayer1;
	bitStream.Read(readPlayer1);
	EXPECT_EQ(readPlayer1, player1);
	
	// Read all 10 values
	for (int i = 0; i < 10; i++) {
		float value;
		bitStream.Read(value);
		float expectedValue = (i == 0) ? 10.5f : 0.0f; // Only first value should be set
		EXPECT_EQ(value, expectedValue);
	}
}

/**
 * Test serialization of an ActivityComponent when not dirty
 */
TEST_F(ActivityTest, ActivityComponentSerializeNotDirtyTest) {
	bitStream.Reset();
	
	// Add a test player 
	LWOOBJID player1 = 100;
	activityComponent->AddActivityPlayerData(player1);
	
	// Do a serialization to reset the dirty flag 
	activityComponent->Serialize(bitStream, false);
	bitStream.Reset(); // Reset bitstream for the actual test
	
	// Now serialize again - should not be dirty this time
	activityComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool isDirty;
	bitStream.Read(isDirty);
	EXPECT_EQ(isDirty, false);
	
	// No additional data should be written when not dirty
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}