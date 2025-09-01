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
	
	// SoundTriggerComponent typically writes minimal data or no data
	// Most sound logic is handled client-side
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0); // Usually empty
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
	
	// Regular updates also typically write no data
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}