#include <gtest/gtest.h>

#include "TriggerComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class TriggerComponentTest : public GameDependenciesTest {
protected:
};

/**
 * Test TriggerComponent serialization for initial update
 */
TEST_F(TriggerComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	TriggerComponent triggerComponent(&testEntity, "");  // Need triggerInfo parameter
	
	RakNet::BitStream bitStream;
	triggerComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// TriggerComponent typically writes minimal data or no data
	// Trigger logic is usually server-side only
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0); // Usually empty
}

/**
 * Test TriggerComponent serialization for regular update
 */
TEST_F(TriggerComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	TriggerComponent triggerComponent(&testEntity, "");  // Need triggerInfo parameter
	
	RakNet::BitStream bitStream;
	triggerComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// Regular updates also typically write no data
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}