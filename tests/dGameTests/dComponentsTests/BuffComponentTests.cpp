#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BuffComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class BuffComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	BuffComponent* buffComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		buffComponent = baseEntity->AddComponent<BuffComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(BuffComponentTest, BuffComponentSerializeInitialEmptyTest) {
	buffComponent->Serialize(bitStream, true);
	
	// Should write false for empty buff list
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	ASSERT_FALSE(hasBuffs);
	
	// That should be all that's written for empty buffs
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 1);
}

TEST_F(BuffComponentTest, BuffComponentSerializeUpdateTest) {
	// Non-initial updates should not write anything
	buffComponent->Serialize(bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

TEST_F(BuffComponentTest, BuffComponentSerializeWithBuffsTest) {
	// Add a test buff
	buffComponent->ApplyBuff(123, 5.0f, baseEntity->GetObjectID());
	
	buffComponent->Serialize(bitStream, true);
	
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	ASSERT_TRUE(hasBuffs);
	
	uint32_t buffCount;
	bitStream.Read(buffCount);
	ASSERT_EQ(buffCount, 1);
	
	// Read the buff data
	uint32_t buffId;
	bitStream.Read(buffId);
	ASSERT_EQ(buffId, 123);
	
	bool hasTime;
	bitStream.Read(hasTime);
	ASSERT_TRUE(hasTime);  // 5.0f != 0.0f
	
	uint32_t timeMs;
	bitStream.Read(timeMs);
	ASSERT_EQ(timeMs, 5000);  // 5.0f * 1000.0f
	
	// Read cancel flags
	bool cancelOnDeath, cancelOnZone, cancelOnDamaged, cancelOnRemoveBuff;
	bool cancelOnUi, cancelOnLogout, cancelOnUnequip, cancelOnDamageAbsorb;
	bitStream.Read(cancelOnDeath);
	bitStream.Read(cancelOnZone);
	bitStream.Read(cancelOnDamaged);
	bitStream.Read(cancelOnRemoveBuff);
	bitStream.Read(cancelOnUi);
	bitStream.Read(cancelOnLogout);
	bitStream.Read(cancelOnUnequip);
	bitStream.Read(cancelOnDamageAbsorb);
	
	// Default values should be false except cancelOnDamageAbsorb which is always false
	ASSERT_FALSE(cancelOnDamageAbsorb);
	
	bool addedByTeammate, applyOnTeammates;
	bitStream.Read(addedByTeammate);
	bitStream.Read(applyOnTeammates);
	
	if (addedByTeammate) {
		uint64_t sourceId;
		bitStream.Read(sourceId);
		ASSERT_EQ(sourceId, baseEntity->GetObjectID());
	}
	
	uint32_t refCount;
	bitStream.Read(refCount);
	ASSERT_EQ(refCount, 1);  // Default reference count
}

TEST_F(BuffComponentTest, BuffComponentMultipleBuffsSerializationTest) {
	// Add multiple buffs
	buffComponent->ApplyBuff(100, 3.0f, baseEntity->GetObjectID());
	buffComponent->ApplyBuff(200, 0.0f, baseEntity->GetObjectID());  // No time
	buffComponent->ApplyBuff(300, 10.5f, baseEntity->GetObjectID());
	
	buffComponent->Serialize(bitStream, true);
	
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	ASSERT_TRUE(hasBuffs);
	
	uint32_t buffCount;
	bitStream.Read(buffCount);
	ASSERT_EQ(buffCount, 3);
	
	// Verify we can read all three buffs
	for (uint32_t i = 0; i < buffCount; i++) {
		uint32_t buffId;
		bitStream.Read(buffId);
		ASSERT_TRUE(buffId == 100 || buffId == 200 || buffId == 300);
		
		bool hasTime;
		bitStream.Read(hasTime);
		
		if (hasTime) {
			uint32_t timeMs;
			bitStream.Read(timeMs);
			ASSERT_TRUE(timeMs > 0);
		}
		
		// Skip other fields for this test
		bool dummy;
		for (int j = 0; j < 8; j++) bitStream.Read(dummy);
		
		bool addedByTeammate;
		bitStream.Read(addedByTeammate);
		bitStream.Read(dummy);  // applyOnTeammates
		
		if (addedByTeammate) {
			uint64_t sourceId;
			bitStream.Read(sourceId);
		}
		
		uint32_t refCount;
		bitStream.Read(refCount);
	}
}

TEST_F(BuffComponentTest, BuffComponentSerializeConsistencyTest) {
	// Test that multiple serializations are consistent
	buffComponent->ApplyBuff(456, 2.5f, baseEntity->GetObjectID());
	
	RakNet::BitStream firstSerialization;
	RakNet::BitStream secondSerialization;
	
	buffComponent->Serialize(firstSerialization, true);
	buffComponent->Serialize(secondSerialization, true);
	
	ASSERT_EQ(firstSerialization.GetNumberOfBitsUsed(), secondSerialization.GetNumberOfBitsUsed());
}