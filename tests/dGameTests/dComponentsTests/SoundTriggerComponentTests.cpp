#include <gtest/gtest.h>

#include "SoundTriggerComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class SoundTriggerComponentTest : public GameDependenciesTest {
protected:
};

/**
 * Test SoundTriggerComponent serialization for initial update
 */
TEST_F(SoundTriggerComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	SoundTriggerComponent soundTriggerComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	soundTriggerComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// SoundTriggerComponent always writes a dirty flag first
	bool hasSoundData;
	ASSERT_TRUE(bitStream.Read(hasSoundData));
	EXPECT_TRUE(hasSoundData); // Should be true for initial update
	
	// Then it writes 5 collection sizes (all 0 for empty component)
	uint8_t musicCuesCount, musicParamsCount, ambientSounds2DCount, ambientSounds3DCount, mixerProgramsCount;
	ASSERT_TRUE(bitStream.Read(musicCuesCount));
	ASSERT_TRUE(bitStream.Read(musicParamsCount));
	ASSERT_TRUE(bitStream.Read(ambientSounds2DCount));
	ASSERT_TRUE(bitStream.Read(ambientSounds3DCount));
	ASSERT_TRUE(bitStream.Read(mixerProgramsCount));
	
	EXPECT_EQ(musicCuesCount, 0);
	EXPECT_EQ(musicParamsCount, 0);
	EXPECT_EQ(ambientSounds2DCount, 0);
	EXPECT_EQ(ambientSounds3DCount, 0);
	EXPECT_EQ(mixerProgramsCount, 0);
}

/**
 * Test SoundTriggerComponent serialization for regular update
 */
TEST_F(SoundTriggerComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	SoundTriggerComponent soundTriggerComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	soundTriggerComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// For regular updates, component writes dirty flag only (should be false)
	bool hasSoundData;
	ASSERT_TRUE(bitStream.Read(hasSoundData));
	EXPECT_FALSE(hasSoundData); // Should be false for regular update with no changes
}