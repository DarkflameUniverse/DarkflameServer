#include <gtest/gtest.h>

#include "BuildBorderComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class BuildBorderComponentTest : public GameDependenciesTest {
protected:
};

/**
 * Test BuildBorderComponent serialization for initial update
 */
TEST_F(BuildBorderComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	BuildBorderComponent buildBorderComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	buildBorderComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// BuildBorderComponent doesn't override Serialize, so it writes nothing
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}

/**
 * Test BuildBorderComponent serialization for regular update (should write nothing)
 */
TEST_F(BuildBorderComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	BuildBorderComponent buildBorderComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	buildBorderComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// For regular updates, BuildBorderComponent writes nothing
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0);
}