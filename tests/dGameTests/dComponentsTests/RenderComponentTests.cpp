#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "RenderComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class RenderComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	RenderComponent* renderComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		renderComponent = baseEntity->AddComponent<RenderComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(RenderComponentTest, RenderComponentSerializeInitialEmptyTest) {
	renderComponent->Serialize(bitStream, true);
	
	// Should write effects count (0 for empty)
	uint32_t effectsCount;
	bitStream.Read(effectsCount);
	ASSERT_EQ(effectsCount, 0);
	
	// That should be all for empty effects
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 32);  // 32 bits for uint32_t
}

TEST_F(RenderComponentTest, RenderComponentSerializeUpdateTest) {
	// Non-initial updates should not write anything
	renderComponent->Serialize(bitStream, false);
	ASSERT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

TEST_F(RenderComponentTest, RenderComponentAddEffectTest) {
	// Add an effect and test serialization
	renderComponent->AddEffect(123, "test_effect", u"fire", 1.0f);
	
	renderComponent->Serialize(bitStream, true);
	
	uint32_t effectsCount;
	bitStream.Read(effectsCount);
	ASSERT_EQ(effectsCount, 1);
	
	// Read effect name
	uint8_t nameSize;
	bitStream.Read(nameSize);
	ASSERT_EQ(nameSize, 11);  // "test_effect" length
	
	std::string effectName;
	for (int i = 0; i < nameSize; i++) {
		uint8_t ch;
		bitStream.Read(ch);
		effectName += static_cast<char>(ch);
	}
	ASSERT_EQ(effectName, "test_effect");
	
	// Read effect ID
	int32_t effectID;
	bitStream.Read(effectID);
	ASSERT_EQ(effectID, 123);
	
	// Read effect type
	uint8_t typeSize;
	bitStream.Read(typeSize);
	ASSERT_EQ(typeSize, 4);  // "fire" length
	
	std::string effectType;
	for (int i = 0; i < typeSize; i++) {
		uint16_t ch;
		bitStream.Read(ch);
		effectType += static_cast<char>(ch);
	}
	ASSERT_EQ(effectType, "fire");
	
	// Read priority and secondary
	float priority;
	int64_t secondary;
	bitStream.Read(priority);
	bitStream.Read(secondary);
	
	ASSERT_EQ(priority, 1.0f);  // Default priority
	ASSERT_EQ(secondary, 0);   // Default secondary
}

TEST_F(RenderComponentTest, RenderComponentMultipleEffectsTest) {
	// Add multiple effects
	renderComponent->AddEffect(100, "effect1", u"water", 1.0f);
	renderComponent->AddEffect(200, "effect2", u"earth", 1.0f);
	
	renderComponent->Serialize(bitStream, true);
	
	uint32_t effectsCount;
	bitStream.Read(effectsCount);
	ASSERT_EQ(effectsCount, 2);
	
	// Just verify we can read both effects without crashing
	for (uint32_t i = 0; i < effectsCount; i++) {
		uint8_t nameSize;
		bitStream.Read(nameSize);
		
		if (nameSize > 0) {
			// Skip name bytes
			for (int j = 0; j < nameSize; j++) {
				uint8_t ch;
				bitStream.Read(ch);
			}
			
			int32_t effectID;
			bitStream.Read(effectID);
			ASSERT_TRUE(effectID == 100 || effectID == 200);
			
			uint8_t typeSize;
			bitStream.Read(typeSize);
			
			// Skip type bytes
			for (int j = 0; j < typeSize; j++) {
				uint16_t ch;
				bitStream.Read(ch);
			}
			
			float priority;
			int64_t secondary;
			bitStream.Read(priority);
			bitStream.Read(secondary);
		}
	}
}

TEST_F(RenderComponentTest, RenderComponentSerializeConsistencyTest) {
	// Test consistency with effects
	renderComponent->AddEffect(456, "consistent", u"air", 1.0f);
	
	RakNet::BitStream firstSerialization;
	RakNet::BitStream secondSerialization;
	
	renderComponent->Serialize(firstSerialization, true);
	renderComponent->Serialize(secondSerialization, true);
	
	ASSERT_EQ(firstSerialization.GetNumberOfBitsUsed(), secondSerialization.GetNumberOfBitsUsed());
}