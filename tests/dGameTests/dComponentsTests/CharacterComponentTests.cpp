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
	
	{
		// Read back the serialized data to verify structure
		bool hasClaimCode0, hasClaimCode1, hasClaimCode2, hasClaimCode3;
		bitStream.Read(hasClaimCode0);
		bitStream.Read(hasClaimCode1);
		bitStream.Read(hasClaimCode2);
		bitStream.Read(hasClaimCode3);
		
		EXPECT_FALSE(hasClaimCode0);  // Default is 0
		EXPECT_FALSE(hasClaimCode1);  // Default is 0
		EXPECT_FALSE(hasClaimCode2);  // Default is 0
		EXPECT_FALSE(hasClaimCode3);  // Default is 0
		
		// Character appearance data
		uint32_t hairColor, hairStyle, head, shirtColor, pantsColor, shirtStyle, headColor;
		uint32_t eyebrows, eyes, mouth;
		uint64_t accountId, lastLogin, propModLastDisplayTime, uScore;
		bool freeToPlay;
		
		bitStream.Read(hairColor);
		bitStream.Read(hairStyle);
		bitStream.Read(head);
		bitStream.Read(shirtColor);
		bitStream.Read(pantsColor);
		bitStream.Read(shirtStyle);
		bitStream.Read(headColor);
		bitStream.Read(eyebrows);
		bitStream.Read(eyes);
		bitStream.Read(mouth);
		bitStream.Read(accountId);
		bitStream.Read(lastLogin);
		bitStream.Read(propModLastDisplayTime);
		bitStream.Read(uScore);
		bitStream.Read(freeToPlay);
		
		// Verify character appearance values
		EXPECT_EQ(hairColor, 5);
		EXPECT_EQ(hairStyle, 10);
		EXPECT_EQ(head, 0);  // Default value
		EXPECT_EQ(shirtColor, 15);
		EXPECT_EQ(pantsColor, 20);
		EXPECT_EQ(shirtStyle, 25);
		EXPECT_EQ(headColor, 0);  // Default value
		EXPECT_EQ(eyebrows, 3);
		EXPECT_EQ(eyes, 7);
		EXPECT_EQ(mouth, 9);
		
		// Account and score values
		EXPECT_GE(accountId, 0);  // Valid account ID
		EXPECT_GE(lastLogin, 0);  // Valid last login
		EXPECT_EQ(propModLastDisplayTime, 0);  // Default value
		EXPECT_EQ(uScore, 12345);  // Value we set
		EXPECT_FALSE(freeToPlay);  // Default value
		
		// Statistics verification - read all statistics values
		uint64_t currencyCollected, bricksCollected, smashablesSmashed, quickBuildsCompleted;
		uint64_t enemiesSmashed, rocketsUsed, missionsCompleted, petsTamed;
		uint64_t imaginationPowerUps, lifePowerUps, armorPowerUps, metersTraveled;
		uint64_t timesSmashed, totalDamageTaken, totalDamageHealed, totalArmorRepaired;
		uint64_t totalImaginationRestored, totalImaginationUsed, distanceDriven, timeAirborneInCar;
		uint64_t racingImaginationPowerUps, racingImaginationCrates, racingCarBoosts;
		uint64_t racingTimesWrecked, racingSmashablesSmashed, racesFinished, firstPlaceRaces;
		
		bitStream.Read(currencyCollected);
		bitStream.Read(bricksCollected);
		bitStream.Read(smashablesSmashed);
		bitStream.Read(quickBuildsCompleted);
		bitStream.Read(enemiesSmashed);
		bitStream.Read(rocketsUsed);
		bitStream.Read(missionsCompleted);
		bitStream.Read(petsTamed);
		bitStream.Read(imaginationPowerUps);
		bitStream.Read(lifePowerUps);
		bitStream.Read(armorPowerUps);
		bitStream.Read(metersTraveled);
		bitStream.Read(timesSmashed);
		bitStream.Read(totalDamageTaken);
		bitStream.Read(totalDamageHealed);
		bitStream.Read(totalArmorRepaired);
		bitStream.Read(totalImaginationRestored);
		bitStream.Read(totalImaginationUsed);
		bitStream.Read(distanceDriven);
		bitStream.Read(timeAirborneInCar);
		bitStream.Read(racingImaginationPowerUps);
		bitStream.Read(racingImaginationCrates);
		bitStream.Read(racingCarBoosts);
		bitStream.Read(racingTimesWrecked);
		bitStream.Read(racingSmashablesSmashed);
		bitStream.Read(racesFinished);
		bitStream.Read(firstPlaceRaces);
		
		// Verify the statistics we set
		EXPECT_EQ(currencyCollected, 100);  // Value we set
		EXPECT_EQ(bricksCollected, 200);    // Value we set  
		EXPECT_EQ(smashablesSmashed, 50);   // Value we set
		EXPECT_EQ(quickBuildsCompleted, 25); // Value we set
		EXPECT_EQ(enemiesSmashed, 75);      // Value we set
		
		// Default values for statistics we didn't set
		EXPECT_EQ(rocketsUsed, 0);
		EXPECT_EQ(missionsCompleted, 0);
		EXPECT_EQ(petsTamed, 0);
		EXPECT_EQ(imaginationPowerUps, 0);
		EXPECT_EQ(lifePowerUps, 0);
		EXPECT_EQ(armorPowerUps, 0);
		
		// Read trailing construction data
		bool unknown1;
		bool isLanding;
		bitStream.Read(unknown1);
		bitStream.Read(isLanding);
		
		EXPECT_FALSE(unknown1);  // Default value
		EXPECT_TRUE(isLanding);  // Should be true when we have rocket config
		
		if (isLanding) {
			uint16_t rocketConfigSize;
			bitStream.Read(rocketConfigSize);
			EXPECT_GT(rocketConfigSize, 0);  // Should have some config data
			
			// Skip over rocket config data
			for (uint16_t i = 0; i < rocketConfigSize; i++) {
				uint16_t configChar;
				bitStream.Read(configChar);
			}
		}
	}
	bitStream.Reset();
}

/**
 * Test serialization of a CharacterComponent update
 */
TEST_F(CharacterComponentTest, CharacterComponentSerializeUpdateTest) {
	// Test non-initial update serialization
	characterComponent->Serialize(bitStream, false);
	
	{
		// Should serialize flags for different update types
		bool hasGMInfo, hasCurrentActivity, hasSocialInfo;
		bitStream.Read(hasGMInfo);
		bitStream.Read(hasCurrentActivity);
		bitStream.Read(hasSocialInfo);
		
		// We set current activity, so that should be true
		EXPECT_TRUE(hasGMInfo);  // PVP was enabled
		EXPECT_TRUE(hasCurrentActivity);  // We set current activity
		EXPECT_FALSE(hasSocialInfo);  // Default state
		
		// Read GM info if present
		if (hasGMInfo) {
			bool pvpEnabled, isGM;
			uint8_t gmLevel;
			bool editorEnabled;
			uint8_t editorLevel;
			
			bitStream.Read(pvpEnabled);
			bitStream.Read(isGM);
			bitStream.Read(gmLevel);
			bitStream.Read(editorEnabled);
			bitStream.Read(editorLevel);
			
			EXPECT_TRUE(pvpEnabled);  // Value we set
			EXPECT_FALSE(isGM);  // Default value
			EXPECT_EQ(gmLevel, 0);  // Default GM level
			EXPECT_FALSE(editorEnabled);  // Default value
			EXPECT_EQ(editorLevel, 0);  // Default editor level
		}
		
		// Read current activity if present
		if (hasCurrentActivity) {
			uint32_t currentActivity;
			bitStream.Read(currentActivity);
			EXPECT_EQ(currentActivity, static_cast<uint32_t>(eGameActivity::QUICKBUILDING));  // Value we set
		}
		
		// Social info should not be present in this test case
		if (hasSocialInfo) {
			// Skip social info reading as it's not expected to be present
			FAIL() << "Social info should not be dirty in this test";
		}
	}
	bitStream.Reset();
}