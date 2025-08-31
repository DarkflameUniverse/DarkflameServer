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
 * Test Construction serialization of a BuffComponent with no buffs
 */
TEST_F(BuffComponentTest, BuffComponentSerializeTest) {
	// Test construction serialization (empty buffs)
	buffComponent->Serialize(bitStream, true);
	
	{
		bool hasBuffs;
		bitStream.Read(hasBuffs);
		EXPECT_FALSE(hasBuffs);
		
		bool immunityBuffs;
		bitStream.Read(immunityBuffs);
		EXPECT_FALSE(immunityBuffs); // Always false according to code
	}
	bitStream.Reset();
	
	// Test update serialization (should write nothing)
	buffComponent->Serialize(bitStream, false);
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

