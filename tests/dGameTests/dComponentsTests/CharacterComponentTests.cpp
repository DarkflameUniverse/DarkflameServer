#include <gtest/gtest.h>

#include "CharacterComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"
#include "Character.h"
#include "User.h"
#include "eGameActivity.h"
#include "eGameMasterLevel.h"
#include "eLootSourceType.h"
#include "MessageType/Game.h"

class CharacterComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		
		// Create a mock user and character
		m_User = std::make_unique<User>(UNASSIGNED_SYSTEM_ADDRESS, "TestUser", "TestPassword");
		
		m_Character = std::make_unique<Character>(1, m_User.get());
		m_Character->SetCoins(1000, eLootSourceType::NONE);
		
		// Set character appearance
		m_Character->SetHairColor(5);
		m_Character->SetHairStyle(10);
		m_Character->SetShirtColor(15);
		m_Character->SetPantsColor(20);
		m_Character->SetShirtStyle(25);
		m_Character->SetEyebrows(30);
		m_Character->SetEyes(35);
		m_Character->SetMouth(40);
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
	
	std::unique_ptr<User> m_User;
	std::unique_ptr<Character> m_Character;
};

TEST_F(CharacterComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	CharacterComponent characterComponent(&testEntity, m_Character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	
	RakNet::BitStream bitStream;
	characterComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	// Claim codes (4 codes)
	for (int i = 0; i < 4; i++) {
		bool hasClaimCode;
		ASSERT_TRUE(bitStream.Read(hasClaimCode));
		EXPECT_FALSE(hasClaimCode); // Default state
	}
	
	// Character appearance
	uint32_t hairColor;
	ASSERT_TRUE(bitStream.Read(hairColor));
	EXPECT_EQ(hairColor, 5);
	
	uint32_t hairStyle;
	ASSERT_TRUE(bitStream.Read(hairStyle));
	EXPECT_EQ(hairStyle, 10);
	
	uint32_t head;
	ASSERT_TRUE(bitStream.Read(head));
	EXPECT_EQ(head, 0); // Default
	
	uint32_t shirtColor;
	ASSERT_TRUE(bitStream.Read(shirtColor));
	EXPECT_EQ(shirtColor, 15);
	
	uint32_t pantsColor;
	ASSERT_TRUE(bitStream.Read(pantsColor));
	EXPECT_EQ(pantsColor, 20);
	
	uint32_t shirtStyle;
	ASSERT_TRUE(bitStream.Read(shirtStyle));
	EXPECT_EQ(shirtStyle, 25);
	
	uint32_t headColor;
	ASSERT_TRUE(bitStream.Read(headColor));
	EXPECT_EQ(headColor, 0); // Default
	
	uint32_t eyebrows;
	ASSERT_TRUE(bitStream.Read(eyebrows));
	EXPECT_EQ(eyebrows, 30);
	
	uint32_t eyes;
	ASSERT_TRUE(bitStream.Read(eyes));
	EXPECT_EQ(eyes, 35);
	
	uint32_t mouth;
	ASSERT_TRUE(bitStream.Read(mouth));
	EXPECT_EQ(mouth, 40);
	
	uint64_t accountID;
	ASSERT_TRUE(bitStream.Read(accountID));
	EXPECT_EQ(accountID, 0); // Default since we can't set it directly
	
	uint64_t lastLogin;
	ASSERT_TRUE(bitStream.Read(lastLogin));
	EXPECT_EQ(lastLogin, 0); // Default since we can't set it directly
	
	uint64_t propModLastDisplayTime;
	ASSERT_TRUE(bitStream.Read(propModLastDisplayTime));
	EXPECT_EQ(propModLastDisplayTime, 0);
	
	uint64_t uscore;
	ASSERT_TRUE(bitStream.Read(uscore));
	EXPECT_EQ(uscore, 0); // Default
	
	bool freeToPlay;
	ASSERT_TRUE(bitStream.Read(freeToPlay));
	EXPECT_FALSE(freeToPlay); // Disabled in DLU
	
	// Stats (23 total statistics)
	for (int i = 0; i < 23; i++) {
		uint64_t stat;
		ASSERT_TRUE(bitStream.Read(stat));
		EXPECT_EQ(stat, 0); // All default to 0
	}
	
	bool hasUnknownFlag;
	ASSERT_TRUE(bitStream.Read(hasUnknownFlag));
	EXPECT_FALSE(hasUnknownFlag); // Always writes 0
	
	bool isLanding;
	ASSERT_TRUE(bitStream.Read(isLanding));
	EXPECT_FALSE(isLanding); // Default state
}

TEST_F(CharacterComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	CharacterComponent characterComponent(&testEntity, m_Character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	
	RakNet::BitStream bitStream;
	characterComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// Should only have the dirty flags
	bool dirtyGMInfo;
	ASSERT_TRUE(bitStream.Read(dirtyGMInfo));
	EXPECT_FALSE(dirtyGMInfo); // Default state
	
	bool dirtyCurrentActivity;
	ASSERT_TRUE(bitStream.Read(dirtyCurrentActivity));
	EXPECT_FALSE(dirtyCurrentActivity); // Default state
	
	bool dirtySocialInfo;
	ASSERT_TRUE(bitStream.Read(dirtySocialInfo));
	EXPECT_FALSE(dirtySocialInfo); // Default state
}

TEST_F(CharacterComponentTest, SerializeWithDirtyGMInfo) {
	Entity testEntity(15, info);
	CharacterComponent characterComponent(&testEntity, m_Character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	
	// Make GM info dirty
	characterComponent.SetPvpEnabled(true);
	characterComponent.SetGMLevel(eGameMasterLevel::JUNIOR_MODERATOR);
	
	RakNet::BitStream bitStream;
	characterComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool dirtyGMInfo;
	ASSERT_TRUE(bitStream.Read(dirtyGMInfo));
	EXPECT_TRUE(dirtyGMInfo);
	
	bool pvpEnabled;
	ASSERT_TRUE(bitStream.Read(pvpEnabled));
	EXPECT_TRUE(pvpEnabled);
	
	bool isGM;
	ASSERT_TRUE(bitStream.Read(isGM));
	EXPECT_TRUE(isGM);
	
	eGameMasterLevel gmLevel;
	ASSERT_TRUE(bitStream.Read(gmLevel));
	EXPECT_EQ(gmLevel, eGameMasterLevel::JUNIOR_MODERATOR);
	
	bool editorEnabled;
	ASSERT_TRUE(bitStream.Read(editorEnabled));
	EXPECT_FALSE(editorEnabled); // Default
	
	eGameMasterLevel editorLevel;
	ASSERT_TRUE(bitStream.Read(editorLevel));
	EXPECT_EQ(editorLevel, eGameMasterLevel::JUNIOR_MODERATOR); // Same as GM level
}

TEST_F(CharacterComponentTest, SerializeWithDirtyCurrentActivity) {
	Entity testEntity(15, info);
	CharacterComponent characterComponent(&testEntity, m_Character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	
	// Set current activity
	characterComponent.SetCurrentActivity(eGameActivity::QUICKBUILDING);
	
	RakNet::BitStream bitStream;
	characterComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool dirtyGMInfo;
	ASSERT_TRUE(bitStream.Read(dirtyGMInfo));
	EXPECT_FALSE(dirtyGMInfo);
	
	bool dirtyCurrentActivity;
	ASSERT_TRUE(bitStream.Read(dirtyCurrentActivity));
	EXPECT_TRUE(dirtyCurrentActivity);
	
	eGameActivity currentActivity;
	ASSERT_TRUE(bitStream.Read(currentActivity));
	EXPECT_EQ(currentActivity, eGameActivity::QUICKBUILDING);
}

TEST_F(CharacterComponentTest, SerializeWithClaimCodes) {
	Entity testEntity(15, info);
	CharacterComponent characterComponent(&testEntity, m_Character.get(), UNASSIGNED_SYSTEM_ADDRESS);
	
	// Set some claim codes via character (need to access private members through Character class)
	// This is more of a conceptual test since claim codes are loaded from XML
	
	RakNet::BitStream bitStream;
	characterComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// Verify claim codes are properly handled (even if they're default values)
	for (int i = 0; i < 4; i++) {
		bool hasClaimCode;
		ASSERT_TRUE(bitStream.Read(hasClaimCode));
		// In default state, all claim codes should be 0/false
		EXPECT_FALSE(hasClaimCode);
	}
}