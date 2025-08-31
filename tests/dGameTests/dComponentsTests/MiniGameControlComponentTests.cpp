#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "MiniGameControlComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class MiniGameControlComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	MiniGameControlComponent* miniGameControlComponent;
	CBITSTREAM
	uint32_t flags = 0;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		miniGameControlComponent = baseEntity->AddComponent<MiniGameControlComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Test serialization behavior (should always write 0x40000000)
TEST_F(MiniGameControlComponentTest, SerializationBehavior) {
	miniGameControlComponent->Serialize(bitStream, true);
	
	// MiniGameControlComponent::Serialize writes a fixed uint32_t value
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 32);
	
	uint32_t value;
	bitStream.Read(value);
	EXPECT_EQ(value, 0x40000000);
	
	bitStream.Reset();
}

// Test serialization with isConstruction = false
TEST_F(MiniGameControlComponentTest, SerializationWithoutConstruction) {
	miniGameControlComponent->Serialize(bitStream, false);
	
	// Should write the same value regardless of isConstruction parameter
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 32);
	
	uint32_t value;
	bitStream.Read(value);
	EXPECT_EQ(value, 0x40000000);
	
	bitStream.Reset();
}

// Test multiple serializations produce consistent results
TEST_F(MiniGameControlComponentTest, ConsistentSerialization) {
	// First serialization
	miniGameControlComponent->Serialize(bitStream, true);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 32);
	uint32_t value1;
	bitStream.Read(value1);
	bitStream.Reset();
	
	// Second serialization
	miniGameControlComponent->Serialize(bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 32);
	uint32_t value2;
	bitStream.Read(value2);
	bitStream.Reset();
	
	// Third serialization
	miniGameControlComponent->Serialize(bitStream, true);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 32);
	uint32_t value3;
	bitStream.Read(value3);
	
	EXPECT_EQ(value1, value2);
	EXPECT_EQ(value2, value3);
	EXPECT_EQ(value1, 0x40000000); // All should be 0x40000000
	
	bitStream.Reset();
}