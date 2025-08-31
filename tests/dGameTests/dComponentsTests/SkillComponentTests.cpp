#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "SkillComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class SkillComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	SkillComponent* skillComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		skillComponent = baseEntity->AddComponent<SkillComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(SkillComponentTest, SkillComponentSerializeInitialTest) {
	skillComponent->Serialize(bitStream, true);
	
	// SkillComponent just writes a 0 bit on initial update
	bool hasSkillData;
	bitStream.Read(hasSkillData);
	ASSERT_FALSE(hasSkillData);  // Should write 0
	
	// Verify that's all that's written
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 1);
}

TEST_F(SkillComponentTest, SkillComponentSerializeUpdateTest) {
	skillComponent->Serialize(bitStream, false);
	
	// Non-initial updates should not write anything for SkillComponent
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

TEST_F(SkillComponentTest, SkillComponentSerializeConsistencyTest) {
	// Test multiple initial serializations are consistent
	RakNet::BitStream firstSerialization;
	RakNet::BitStream secondSerialization;
	
	skillComponent->Serialize(firstSerialization, true);
	skillComponent->Serialize(secondSerialization, true);
	
	ASSERT_EQ(firstSerialization.GetNumberOfBitsUsed(), secondSerialization.GetNumberOfBitsUsed());
	ASSERT_EQ(firstSerialization.GetNumberOfBitsUsed(), 1);
	
	bool hasSkillData1, hasSkillData2;
	firstSerialization.Read(hasSkillData1);
	secondSerialization.Read(hasSkillData2);
	ASSERT_EQ(hasSkillData1, hasSkillData2);
	ASSERT_FALSE(hasSkillData1);
}

TEST_F(SkillComponentTest, SkillComponentUniqueIdTest) {
	// Test unique skill ID generation
	uint32_t id1 = skillComponent->GetUniqueSkillId();
	uint32_t id2 = skillComponent->GetUniqueSkillId();
	uint32_t id3 = skillComponent->GetUniqueSkillId();
	
	ASSERT_NE(id1, id2);
	ASSERT_NE(id2, id3);
	ASSERT_NE(id1, id3);
	
	// Should be sequential
	ASSERT_EQ(id2, id1 + 1);
	ASSERT_EQ(id3, id2 + 1);
}