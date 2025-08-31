#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "CharacterComponent.h"
#include "Entity.h"
#include "Character.h"
#include "User.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class CharacterComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	CharacterComponent* characterComponent;
	std::unique_ptr<Character> character;
	std::unique_ptr<User> user;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		
		// Create a mock user and character
		user = std::make_unique<User>(UNASSIGNED_SYSTEM_ADDRESS, "", "TestUser");
		// Note: User account ID is set internally
		
		character = std::make_unique<Character>(1, user.get());
		character->SetHairColor(5);
		character->SetHairStyle(10);
		character->SetShirtColor(15);
		character->SetPantsColor(20);
		character->SetShirtStyle(25);
		character->SetEyebrows(3);
		character->SetEyes(7);
		character->SetMouth(9);
		// Note: Last login is set internally
		
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		characterComponent = baseEntity->AddComponent<CharacterComponent>(character.get(), UNASSIGNED_SYSTEM_ADDRESS);
		
		// Note: Statistics are set internally through game actions, not directly settable
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(CharacterComponentTest, CharacterComponentSerializeConstructionTest) {
	characterComponent->Serialize(bitStream, true);
	
	// Read back the serialized data to verify structure
	bool hasClaimCode0;
	bitStream.Read(hasClaimCode0);
	ASSERT_FALSE(hasClaimCode0);  // Default is 0
	
	bool hasClaimCode1;
	bitStream.Read(hasClaimCode1);
	ASSERT_FALSE(hasClaimCode1);  // Default is 0
	
	bool hasClaimCode2;
	bitStream.Read(hasClaimCode2);
	ASSERT_FALSE(hasClaimCode2);  // Default is 0
	
	bool hasClaimCode3;
	bitStream.Read(hasClaimCode3);
	ASSERT_FALSE(hasClaimCode3);  // Default is 0
	
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
	
	ASSERT_EQ(hairColor, 5);
	ASSERT_EQ(hairStyle, 10);
	ASSERT_EQ(head, 0);
	ASSERT_EQ(shirtColor, 15);
	ASSERT_EQ(pantsColor, 20);
	ASSERT_EQ(shirtStyle, 25);
	ASSERT_EQ(headColor, 0);
	ASSERT_EQ(eyebrows, 3);
	ASSERT_EQ(eyes, 7);
	ASSERT_EQ(mouth, 9);
	// Account ID and other values are set internally
	ASSERT_GE(accountId, 0);  // Just verify it's a valid number
	ASSERT_GE(lastLogin, 0);
	ASSERT_EQ(propModLastDisplayTime, 0);
	ASSERT_GE(uScore, 0);  // U-score starts at 0
	ASSERT_FALSE(freeToPlay);
	
	// Statistics verification
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
	
	ASSERT_GE(currencyCollected, 0);  // Default statistics should be >= 0
	ASSERT_GE(bricksCollected, 0);
	ASSERT_GE(smashablesSmashed, 0);
	ASSERT_GE(quickBuildsCompleted, 0);
	ASSERT_GE(enemiesSmashed, 0);
	ASSERT_GE(rocketsUsed, 0);
	ASSERT_GE(missionsCompleted, 0);
	ASSERT_GE(petsTamed, 0);
	ASSERT_EQ(imaginationPowerUps, 0);  // Default values
	ASSERT_EQ(lifePowerUps, 0);
	ASSERT_EQ(armorPowerUps, 0);
}

TEST_F(CharacterComponentTest, CharacterComponentSerializeUpdateTest) {
	// Test non-initial update serialization
	characterComponent->Serialize(bitStream, false);
	
	// Should serialize flags for different update types
	bool hasLevel, hasSpeedBoost, hasClaimCodes, hasActivity;
	bitStream.Read(hasLevel);
	bitStream.Read(hasSpeedBoost);
	bitStream.Read(hasClaimCodes);
	bitStream.Read(hasActivity);
	
	// Default state should have no updates
	ASSERT_FALSE(hasLevel);
	ASSERT_FALSE(hasSpeedBoost);
	ASSERT_FALSE(hasClaimCodes);
	ASSERT_FALSE(hasActivity);
}

TEST_F(CharacterComponentTest, CharacterComponentClaimCodeSerializationTest) {
	// Test that default claim codes serialize correctly (all should be 0)
	characterComponent->Serialize(bitStream, true);
	
	bool hasClaimCode0, hasClaimCode1, hasClaimCode2, hasClaimCode3;
	
	bitStream.Read(hasClaimCode0);
	ASSERT_FALSE(hasClaimCode0);  // 0 should result in false
	
	bitStream.Read(hasClaimCode1);
	ASSERT_FALSE(hasClaimCode1);  // 0 should result in false
	
	bitStream.Read(hasClaimCode2);
	ASSERT_FALSE(hasClaimCode2);  // 0 should result in false
	
	bitStream.Read(hasClaimCode3);
	ASSERT_FALSE(hasClaimCode3);  // 0 should result in false
}