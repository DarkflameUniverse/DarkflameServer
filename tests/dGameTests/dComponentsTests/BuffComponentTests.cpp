#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BuffComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class BuffComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	BuffComponent* buffComponent;
	CBITSTREAM
	uint32_t flags = 0;
	
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		buffComponent = baseEntity->AddComponent<BuffComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test BuffComponent serialization with no buffs applied
 */
TEST_F(BuffComponentTest, BuffComponentSerializeNoBuffsTest) {
	bitStream.Reset();
	
	// With no buffs, should serialize empty state
	buffComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	EXPECT_EQ(hasBuffs, false); // No buffs applied
	
	bool immunityData;
	bitStream.Read(immunityData);
	EXPECT_EQ(immunityData, false); // Immunity data bit should be false
}

/**
 * Test BuffComponent serialization with single buff applied
 */
TEST_F(BuffComponentTest, BuffComponentSerializeSingleBuffTest) {
	bitStream.Reset();
	
	// Apply a buff with specific properties
	int32_t buffId = 123;
	float duration = 5.0f;
	LWOOBJID source = 9876;
	
	buffComponent->ApplyBuff(buffId, duration, source, false, false, false, false, false, true, true, false, false);
	buffComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	EXPECT_EQ(hasBuffs, true); // Has buffs
	
	uint32_t buffCount;
	bitStream.Read(buffCount);
	EXPECT_EQ(buffCount, 1); // Should have 1 buff
	
	// Read buff data
	uint32_t serializedBuffId;
	bitStream.Read(serializedBuffId);
	EXPECT_EQ(serializedBuffId, buffId);
	
	bool hasTime;
	bitStream.Read(hasTime);
	EXPECT_EQ(hasTime, true); // Buff has time
	
	uint32_t timeMs;
	bitStream.Read(timeMs);
	EXPECT_GT(timeMs, 0); // Should have positive time (approx 5000ms but could be slightly less due to timing)
	EXPECT_LE(timeMs, 5000); // Should not exceed initial duration
	
	// Read cancel flags in order they're written
	bool cancelOnDeath;
	bitStream.Read(cancelOnDeath);
	EXPECT_EQ(cancelOnDeath, false); // Set to false in ApplyBuff call
	
	bool cancelOnZone;
	bitStream.Read(cancelOnZone);
	EXPECT_EQ(cancelOnZone, false); // Set to false in ApplyBuff call
	
	bool cancelOnDamaged;
	bitStream.Read(cancelOnDamaged);
	EXPECT_EQ(cancelOnDamaged, false); // Set to false in ApplyBuff call
	
	bool cancelOnRemoveBuff;
	bitStream.Read(cancelOnRemoveBuff);
	EXPECT_EQ(cancelOnRemoveBuff, false); // Set to false in ApplyBuff call
	
	bool cancelOnUi;
	bitStream.Read(cancelOnUi);
	EXPECT_EQ(cancelOnUi, true); // Set to true in ApplyBuff call
	
	bool cancelOnLogout;
	bitStream.Read(cancelOnLogout);
	EXPECT_EQ(cancelOnLogout, false); // Set to false in ApplyBuff call
	
	bool cancelOnUnequip;
	bitStream.Read(cancelOnUnequip);
	EXPECT_EQ(cancelOnUnequip, true); // Set to true in ApplyBuff call
	
	bool cancelOnDamageAbsorbRanOut;
	bitStream.Read(cancelOnDamageAbsorbRanOut);
	EXPECT_EQ(cancelOnDamageAbsorbRanOut, false); // Always false
	
	bool addedByTeammate;
	bitStream.Read(addedByTeammate);
	EXPECT_EQ(addedByTeammate, false); // No team setup in test
	
	bool applyOnTeammates;
	bitStream.Read(applyOnTeammates);
	EXPECT_EQ(applyOnTeammates, false); // Set to false in ApplyBuff call
	
	uint32_t refCount;
	bitStream.Read(refCount);
	EXPECT_EQ(refCount, 1); // Should have ref count of 1
	
	bool immunityData;
	bitStream.Read(immunityData);
	EXPECT_EQ(immunityData, false); // Immunity data bit should be false
}

/**
 * Test BuffComponent serialization with multiple buffs
 */
TEST_F(BuffComponentTest, BuffComponentSerializeMultipleBuffsTest) {
	bitStream.Reset();
	
	// Apply multiple buffs
	buffComponent->ApplyBuff(100, 3.0f, 1000);
	buffComponent->ApplyBuff(200, 0.0f, 2000); // Permanent buff (time = 0)
	buffComponent->ApplyBuff(300, 10.0f, 3000);
	
	buffComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	bool hasBuffs;
	bitStream.Read(hasBuffs);
	EXPECT_EQ(hasBuffs, true);
	
	uint32_t buffCount;
	bitStream.Read(buffCount);
	EXPECT_EQ(buffCount, 3); // Should have 3 buffs
	
	// Read first buff (ID 100)
	uint32_t buffId1;
	bitStream.Read(buffId1);
	EXPECT_EQ(buffId1, 100);
	
	bool hasTime1;
	bitStream.Read(hasTime1);
	EXPECT_EQ(hasTime1, true);
	
	uint32_t timeMs1;
	bitStream.Read(timeMs1);
	EXPECT_GT(timeMs1, 0);
	EXPECT_LE(timeMs1, 3000);
	
	// Skip the rest of first buff's flags (8 bools + team stuff + refcount)
	bool dummy;
	for (int i = 0; i < 8; i++) bitStream.Read(dummy); // cancel flags
	bitStream.Read(dummy); // addedByTeammate
	bitStream.Read(dummy); // applyOnTeammates
	uint32_t dummyInt;
	bitStream.Read(dummyInt); // refCount
	
	// Read second buff (ID 200) - permanent buff
	uint32_t buffId2;
	bitStream.Read(buffId2);
	EXPECT_EQ(buffId2, 200);
	
	bool hasTime2;
	bitStream.Read(hasTime2);
	EXPECT_EQ(hasTime2, false); // Permanent buff has no time
	
	// Skip the rest of second buff's flags
	for (int i = 0; i < 8; i++) bitStream.Read(dummy);
	bitStream.Read(dummy);
	bitStream.Read(dummy);
	bitStream.Read(dummyInt);
	
	// Read third buff (ID 300)
	uint32_t buffId3;
	bitStream.Read(buffId3);
	EXPECT_EQ(buffId3, 300);
	
	bool hasTime3;
	bitStream.Read(hasTime3);
	EXPECT_EQ(hasTime3, true);
	
	uint32_t timeMs3;
	bitStream.Read(timeMs3);
	EXPECT_GT(timeMs3, 0);
	EXPECT_LE(timeMs3, 10000);
	
	// Skip the rest of third buff's flags
	for (int i = 0; i < 8; i++) bitStream.Read(dummy);
	bitStream.Read(dummy);
	bitStream.Read(dummy);
	bitStream.Read(dummyInt);
	
	bool immunityData;
	bitStream.Read(immunityData);
	EXPECT_EQ(immunityData, false);
}

/**
 * Test BuffComponent regular update serialization (should not serialize)
 */
TEST_F(BuffComponentTest, BuffComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Apply a buff first
	buffComponent->ApplyBuff(123, 5.0f, 9876);
	
	// Regular update should not serialize anything
	buffComponent->Serialize(bitStream, false);
	
	// BitStream should be empty for regular updates
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}