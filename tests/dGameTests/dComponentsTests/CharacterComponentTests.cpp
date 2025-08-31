#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "Character.h"
#include "User.h"
#include "eReplicaComponentType.h"
#include "eGameActivity.h"

class CharacterComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	CharacterComponent* characterComponent;
	std::unique_ptr<Character> character;
	std::unique_ptr<User> user;
	CBITSTREAM
	uint32_t flags = 0;

	void SetUp() override {
		SetUpDependencies();
		
		// Create a mock user and character
		user = std::make_unique<User>(UNASSIGNED_SYSTEM_ADDRESS, "", "TestUser");
		
		character = std::make_unique<Character>(1, user.get());
		character->SetHairColor(5);
		character->SetHairStyle(10);
		character->SetShirtColor(15);
		character->SetPantsColor(20);
		character->SetShirtStyle(25);
		character->SetEyebrows(3);
		character->SetEyes(7);
		character->SetMouth(9);
		
		baseEntity = new Entity(15, GameDependenciesTest::info);
		characterComponent = baseEntity->AddComponent<CharacterComponent>(character.get(), UNASSIGNED_SYSTEM_ADDRESS);
		
		// Set some test values using available setter methods
		characterComponent->SetUScore(12345);
		characterComponent->SetCurrentActivity(eGameActivity::QUICKBUILDING);
		characterComponent->SetReputation(6789);
		characterComponent->SetPvpEnabled(true);
		characterComponent->SetLastRocketConfig(u"test,rocket,config");
		
		// Update some statistics using the UpdatePlayerStatistic method
		characterComponent->UpdatePlayerStatistic(StatisticID::CurrencyCollected, 100);
		characterComponent->UpdatePlayerStatistic(StatisticID::BricksCollected, 200);
		characterComponent->UpdatePlayerStatistic(StatisticID::SmashablesSmashed, 50);
		characterComponent->UpdatePlayerStatistic(StatisticID::QuickBuildsCompleted, 25);
		characterComponent->UpdatePlayerStatistic(StatisticID::EnemiesSmashed, 75);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test Construction of a CharacterComponent
 */
TEST_F(CharacterComponentTest, CharacterComponentSerializeConstructionTest) {
	characterComponent->Serialize(bitStream, true);
	
	// Basic check that construction serialization produces data
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
	
	// Basic structure validation - should be able to read claim codes flags
	bool hasClaimCode0 = false, hasClaimCode1 = false, hasClaimCode2 = false, hasClaimCode3 = false;
	bitStream.Read(hasClaimCode0);
	bitStream.Read(hasClaimCode1);
	bitStream.Read(hasClaimCode2);
	bitStream.Read(hasClaimCode3);
	
	// All claim codes should be false by default
	EXPECT_FALSE(hasClaimCode0);
	EXPECT_FALSE(hasClaimCode1);
	EXPECT_FALSE(hasClaimCode2);
	EXPECT_FALSE(hasClaimCode3);
	
	bitStream.Reset();
}

/**
 * Test serialization of a CharacterComponent update
 */
TEST_F(CharacterComponentTest, CharacterComponentSerializeUpdateTest) {
	// Test non-initial update serialization
	characterComponent->Serialize(bitStream, false);
	
	// Basic check that update serialization produces data
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0);
	
	// Should serialize flags for different update types
	bool hasGMInfo = false, hasCurrentActivity = false, hasSocialInfo = false;
	bitStream.Read(hasGMInfo);
	bitStream.Read(hasCurrentActivity);
	bitStream.Read(hasSocialInfo);
	
	// Verify the flags reflect our changes - we set PVP and current activity
	EXPECT_TRUE(hasGMInfo);  // PVP was enabled in setup
	EXPECT_TRUE(hasCurrentActivity);  // Current activity was set in setup
	EXPECT_FALSE(hasSocialInfo);  // Not modified in our setup
	
	bitStream.Reset();
}