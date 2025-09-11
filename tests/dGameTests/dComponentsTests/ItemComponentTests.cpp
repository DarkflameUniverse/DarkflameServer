#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "ItemComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class ItemComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	ItemComponent* itemComponent;
	CBITSTREAM
	uint32_t flags = 0;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		itemComponent = baseEntity->AddComponent<ItemComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Test serialization behavior (should just write a single 0 bit)
TEST_F(ItemComponentTest, SerializationBehavior) {
	itemComponent->Serialize(bitStream, true);
	
	// ItemComponent::Serialize just writes a single 0 bit
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 1);
	
	bool value;
	bitStream.Read(value);
	EXPECT_FALSE(value); // Should be false (0 bit)
	
	bitStream.Reset();
}

// Test serialization with isConstruction = false
TEST_F(ItemComponentTest, SerializationWithoutConstruction) {
	itemComponent->Serialize(bitStream, false);
	
	// Should still just write a single 0 bit regardless of isConstruction parameter
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 1);
	
	bool value;
	bitStream.Read(value);
	EXPECT_FALSE(value); // Should be false (0 bit)
	
	bitStream.Reset();
}

// Test multiple serializations produce consistent results
TEST_F(ItemComponentTest, ConsistentSerialization) {
	// First serialization
	itemComponent->Serialize(bitStream, true);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 1);
	bool value1;
	bitStream.Read(value1);
	bitStream.Reset();
	
	// Second serialization
	itemComponent->Serialize(bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 1);
	bool value2;
	bitStream.Read(value2);
	bitStream.Reset();
	
	// Third serialization
	itemComponent->Serialize(bitStream, true);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 1);
	bool value3;
	bitStream.Read(value3);
	
	EXPECT_EQ(value1, value2);
	EXPECT_EQ(value2, value3);
	EXPECT_FALSE(value1); // All should be false
	
	bitStream.Reset();
}