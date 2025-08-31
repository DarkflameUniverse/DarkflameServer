#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "RenderComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class RenderComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	RenderComponent* renderComponent;
	CBITSTREAM
	uint32_t flags = 0;
	
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		renderComponent = baseEntity->AddComponent<RenderComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test RenderComponent serialization with no effects
 */
TEST_F(RenderComponentTest, RenderComponentSerializeNoEffectsTest) {
	bitStream.Reset();
	
	// Test initial update with no effects
	renderComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	uint32_t effectCount;
	bitStream.Read(effectCount);
	EXPECT_EQ(effectCount, 0); // No effects added
}

/**
 * Test RenderComponent serialization with single effect
 */
TEST_F(RenderComponentTest, RenderComponentSerializeSingleEffectTest) {
	bitStream.Reset();
	
	// Add a single effect
	std::string effectName = "testEffect";
	std::u16string effectType = u"testType";
	int32_t effectId = 123;
	float priority = 1.5f;
	
	renderComponent->AddEffect(effectId, effectName, effectType, priority);
	renderComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	uint32_t effectCount;
	bitStream.Read(effectCount);
	EXPECT_EQ(effectCount, 1); // One effect added
	
	// Read effect name length
	uint8_t nameLength;
	bitStream.Read(nameLength);
	EXPECT_EQ(nameLength, effectName.size());
	
	// Read effect name
	std::string readName;
	readName.resize(nameLength);
	for (uint8_t i = 0; i < nameLength; i++) {
		uint8_t ch;
		bitStream.Read(ch);
		readName[i] = static_cast<char>(ch);
	}
	EXPECT_EQ(readName, effectName);
	
	// Read effect ID
	int32_t readEffectId;
	bitStream.Read(readEffectId);
	EXPECT_EQ(readEffectId, effectId);
	
	// Read effect type length
	uint8_t typeLength;
	bitStream.Read(typeLength);
	EXPECT_EQ(typeLength, effectType.size());
	
	// Read effect type
	std::u16string readType;
	readType.resize(typeLength);
	for (uint8_t i = 0; i < typeLength; i++) {
		uint16_t ch;
		bitStream.Read(ch);
		readType[i] = static_cast<char16_t>(ch);
	}
	EXPECT_EQ(readType, effectType);
	
	// Read priority
	float readPriority;
	bitStream.Read(readPriority);
	EXPECT_EQ(readPriority, priority);
	
	// Read secondary (should be 0 by default)
	int64_t secondary;
	bitStream.Read(secondary);
	EXPECT_EQ(secondary, 0);
}

/**
 * Test RenderComponent serialization with multiple effects
 */
TEST_F(RenderComponentTest, RenderComponentSerializeMultipleEffectsTest) {
	bitStream.Reset();
	
	// Add multiple effects
	renderComponent->AddEffect(100, "effect1", u"type1", 1.0f);
	renderComponent->AddEffect(200, "effect2", u"type2", 2.0f);
	renderComponent->AddEffect(300, "effect3", u"type3", 3.0f);
	
	renderComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	uint32_t effectCount;
	bitStream.Read(effectCount);
	EXPECT_EQ(effectCount, 3); // Three effects added
	
	// Read first effect
	uint8_t nameLength1;
	bitStream.Read(nameLength1);
	EXPECT_EQ(nameLength1, 7); // "effect1"
	
	// Skip reading the detailed content of all effects for brevity
	// Just verify that the count is correct and we can read some basic data
	
	for (uint8_t i = 0; i < nameLength1; i++) {
		uint8_t dummy;
		bitStream.Read(dummy);
	}
	
	int32_t effectId1;
	bitStream.Read(effectId1);
	EXPECT_EQ(effectId1, 100);
	
	// Skip the rest of the first effect and the other effects
	// The important part is that the count was correct
}

/**
 * Test RenderComponent serialization with empty effect name
 */
TEST_F(RenderComponentTest, RenderComponentSerializeEmptyEffectNameTest) {
	bitStream.Reset();
	
	// Add an effect with empty name
	renderComponent->AddEffect(456, "", u"emptyNameType", 0.5f);
	renderComponent->Serialize(bitStream, true);
	
	// Read back the serialized data
	uint32_t effectCount;
	bitStream.Read(effectCount);
	EXPECT_EQ(effectCount, 1); // One effect added
	
	// Read effect name length
	uint8_t nameLength;
	bitStream.Read(nameLength);
	EXPECT_EQ(nameLength, 0); // Empty name
	
	// According to the code, if name is empty, nothing else is written
	// So the stream should end here for this effect
}

/**
 * Test RenderComponent regular update serialization (should not serialize)
 */
TEST_F(RenderComponentTest, RenderComponentSerializeRegularUpdateTest) {
	bitStream.Reset();
	
	// Add an effect first
	renderComponent->AddEffect(789, "regularUpdate", u"regularType", 1.0f);
	
	// Regular update should not serialize anything
	renderComponent->Serialize(bitStream, false);
	
	// BitStream should be empty for regular updates
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}