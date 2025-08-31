#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "CollectibleComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class CollectibleTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	CollectibleComponent* collectibleComponent;
	CBITSTREAM
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		collectibleComponent = baseEntity->AddComponent<CollectibleComponent>(123); // Test with collectibleId = 123
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test basic CollectibleComponent serialization
 */
TEST_F(CollectibleTest, CollectibleComponentSerializeBasicTest) {
	bitStream.Reset();
	
	// Serialize the collectible component
	collectibleComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	int16_t collectibleId;
	bitStream.Read(collectibleId);
	EXPECT_EQ(collectibleId, 123); // Should match the ID we set
	
	// Verify getter
	EXPECT_EQ(collectibleComponent->GetCollectibleId(), 123);
}

/**
 * Test CollectibleComponent serialization with construction flag true
 */
TEST_F(CollectibleTest, CollectibleComponentSerializeConstructionTest) {
	bitStream.Reset();
	
	// Serialize with construction = true
	collectibleComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	int16_t collectibleId;
	bitStream.Read(collectibleId);
	EXPECT_EQ(collectibleId, 123);
}

/**
 * Test CollectibleComponent serialization with construction flag false
 */
TEST_F(CollectibleTest, CollectibleComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Serialize with construction = false
	collectibleComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	int16_t collectibleId;
	bitStream.Read(collectibleId);
	EXPECT_EQ(collectibleId, 123); // Should still serialize the same way
}

/**
 * Test CollectibleComponent with different collectible IDs
 */
TEST_F(CollectibleTest, CollectibleComponentDifferentIDsTest) {
	// Create another entity with a different collectible ID
	Entity* anotherEntity = new Entity(16, GameDependenciesTest::info);
	CollectibleComponent* anotherCollectible = anotherEntity->AddComponent<CollectibleComponent>(456);
	
	bitStream.Reset();
	
	// Serialize the first collectible
	collectibleComponent->Serialize(bitStream, true);
	
	int16_t firstId;
	bitStream.Read(firstId);
	EXPECT_EQ(firstId, 123);
	
	bitStream.Reset();
	
	// Serialize the second collectible
	anotherCollectible->Serialize(bitStream, true);
	
	int16_t secondId;
	bitStream.Read(secondId);
	EXPECT_EQ(secondId, 456);
	
	// Verify getters
	EXPECT_EQ(collectibleComponent->GetCollectibleId(), 123);
	EXPECT_EQ(anotherCollectible->GetCollectibleId(), 456);
	
	delete anotherEntity;
}