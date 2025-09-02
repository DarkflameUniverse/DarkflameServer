#include <gtest/gtest.h>

#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "ScriptComponent.h"

class ScriptComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(ScriptComponentTest, SerializeInitialUpdateNoNetworkSettings) {
	Entity testEntity(15, info);
	ScriptComponent scriptComponent(&testEntity, "", false);
	
	RakNet::BitStream bitStream;
	scriptComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	bool hasNetworkSettings;
	ASSERT_TRUE(bitStream.Read(hasNetworkSettings));
	EXPECT_FALSE(hasNetworkSettings); // No network settings by default
}

TEST_F(ScriptComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	ScriptComponent scriptComponent(&testEntity, "", false);
	
	RakNet::BitStream bitStream;
	scriptComponent.Serialize(bitStream, false);
	
	// Regular updates should not write anything for ScriptComponent
	bitStream.ResetReadPointer();
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(ScriptComponentTest, SerializeWithNetworkSettings) {
	Entity testEntity(15, info);
	
	// Add some network settings to the entity
	testEntity.SetNetworkVar<float>(u"test_float", 123.45f);
	testEntity.SetNetworkVar<int32_t>(u"test_int", 42);
	
	ScriptComponent scriptComponent(&testEntity, "", false);
	
	RakNet::BitStream bitStream;
	scriptComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	bool hasNetworkSettings;
	ASSERT_TRUE(bitStream.Read(hasNetworkSettings));
	EXPECT_TRUE(hasNetworkSettings); // Should have network settings
	
	uint32_t ldfDataSize;
	ASSERT_TRUE(bitStream.Read(ldfDataSize));
	EXPECT_GT(ldfDataSize, 0); // Should have some data
	
	// Verify the LDF data is present
	RakNet::BitStream ldfStream;
	ASSERT_TRUE(bitStream.Read(ldfStream, ldfDataSize));
	
	ldfStream.ResetReadPointer();
	
	uint8_t ldfType;
	ASSERT_TRUE(ldfStream.Read(ldfType));
	EXPECT_EQ(ldfType, 0); // Always writes 0 first
	
	uint32_t settingsCount;
	ASSERT_TRUE(ldfStream.Read(settingsCount));
	EXPECT_EQ(settingsCount, 2); // We added 2 settings
}

TEST_F(ScriptComponentTest, SerializeConsistentBehavior) {
	Entity testEntity(15, info);
	ScriptComponent scriptComponent(&testEntity, "", false);
	
	// Test that multiple serializations are consistent
	RakNet::BitStream firstStream, secondStream;
	
	scriptComponent.Serialize(firstStream, true);
	scriptComponent.Serialize(secondStream, true);
	
	EXPECT_EQ(firstStream.GetNumberOfBitsUsed(), secondStream.GetNumberOfBitsUsed());
	
	firstStream.ResetReadPointer();
	secondStream.ResetReadPointer();
	
	bool firstHasSettings, secondHasSettings;
	ASSERT_TRUE(firstStream.Read(firstHasSettings));
	ASSERT_TRUE(secondStream.Read(secondHasSettings));
	EXPECT_EQ(firstHasSettings, secondHasSettings);
}

TEST_F(ScriptComponentTest, SerializeScriptNameHandling) {
	Entity testEntity(15, info);
	
	// Test with different script names - serialization shouldn't change based on script name
	ScriptComponent scriptComponent1(&testEntity, "TestScript", false);
	ScriptComponent scriptComponent2(&testEntity, "AnotherScript", false);
	
	RakNet::BitStream stream1, stream2;
	
	scriptComponent1.Serialize(stream1, true);
	scriptComponent2.Serialize(stream2, true);
	
	// Serialization should be the same regardless of script name since 
	// script names are not serialized, only network settings
	stream1.ResetReadPointer();
	stream2.ResetReadPointer();
	
	bool hasSettings1, hasSettings2;
	ASSERT_TRUE(stream1.Read(hasSettings1));
	ASSERT_TRUE(stream2.Read(hasSettings2));
	EXPECT_EQ(hasSettings1, hasSettings2);
	EXPECT_FALSE(hasSettings1); // Both should be false without network settings
}

TEST_F(ScriptComponentTest, SerializeSerializedFlag) {
	Entity testEntity(15, info);
	ScriptComponent scriptComponent(&testEntity, "", false);
	
	// Test the serialized flag functionality
	scriptComponent.SetSerialized(true); // Should accept this call
	
	// The serialized flag itself doesn't affect the Serialize method output,
	// but it's tracked by the component
	RakNet::BitStream bitStream;
	scriptComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	bool hasNetworkSettings;
	ASSERT_TRUE(bitStream.Read(hasNetworkSettings));
	EXPECT_FALSE(hasNetworkSettings); // Still no network settings
}