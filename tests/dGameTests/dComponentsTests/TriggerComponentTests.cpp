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
	TriggerComponent triggerComponent(&testEntity, "0:0");  // Valid triggerInfo format
	
	RakNet::BitStream bitStream;
	triggerComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// TriggerComponent doesn't override Serialize, so it writes nothing
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

/**
 * Test TriggerComponent serialization for regular update
 */
TEST_F(TriggerComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	TriggerComponent triggerComponent(&testEntity, "0:0");  // Valid triggerInfo format
	
	RakNet::BitStream bitStream;
	triggerComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// TriggerComponent doesn't override Serialize, so it writes nothing
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}