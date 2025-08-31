#include <gtest/gtest.h>

#include "SkillComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class SkillComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(SkillComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	SkillComponent skillComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	skillComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	bool skillFlag;
	ASSERT_TRUE(bitStream.Read(skillFlag));
	EXPECT_FALSE(skillFlag); // Always writes 0 for initial update
}

TEST_F(SkillComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	SkillComponent skillComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	skillComponent.Serialize(bitStream, false);
	
	// Regular updates should not write anything for SkillComponent
	bitStream.ResetReadPointer();
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(SkillComponentTest, SerializeConsistentBehavior) {
	Entity testEntity(15, info);
	SkillComponent skillComponent(&testEntity);
	
	// Test that multiple initial serializations are consistent
	RakNet::BitStream firstStream, secondStream;
	
	skillComponent.Serialize(firstStream, true);
	skillComponent.Serialize(secondStream, true);
	
	EXPECT_EQ(firstStream.GetNumberOfBitsUsed(), secondStream.GetNumberOfBitsUsed());
	EXPECT_EQ(firstStream.GetNumberOfBitsUsed(), 1); // Should always be 1 bit (false)
	
	firstStream.ResetReadPointer();
	secondStream.ResetReadPointer();
	
	bool firstFlag, secondFlag;
	ASSERT_TRUE(firstStream.Read(firstFlag));
	ASSERT_TRUE(secondStream.Read(secondFlag));
	EXPECT_FALSE(firstFlag);
	EXPECT_FALSE(secondFlag);
	EXPECT_EQ(firstFlag, secondFlag);
}

TEST_F(SkillComponentTest, GetUniqueSkillId) {
	Entity testEntity(15, info);
	SkillComponent skillComponent(&testEntity);
	
	// Test that unique skill IDs increment
	uint32_t firstId = skillComponent.GetUniqueSkillId();
	uint32_t secondId = skillComponent.GetUniqueSkillId();
	uint32_t thirdId = skillComponent.GetUniqueSkillId();
	
	EXPECT_EQ(secondId, firstId + 1);
	EXPECT_EQ(thirdId, firstId + 2);
	EXPECT_GT(firstId, 0); // Should start from at least 1
}

TEST_F(SkillComponentTest, SerializeAfterSkillUse) {
	Entity testEntity(15, info);
	SkillComponent skillComponent(&testEntity);
	
	// Generate some skill IDs to simulate skill usage
	skillComponent.GetUniqueSkillId();
	skillComponent.GetUniqueSkillId();
	skillComponent.GetUniqueSkillId();
	
	// Serialization behavior should still be the same
	RakNet::BitStream bitStream;
	skillComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	bool skillFlag;
	ASSERT_TRUE(bitStream.Read(skillFlag));
	EXPECT_FALSE(skillFlag); // Still writes 0 regardless of internal state
}