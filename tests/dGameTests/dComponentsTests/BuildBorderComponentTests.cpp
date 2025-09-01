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
	
	// BuildBorderComponent always writes true for initial update
	bool hasBorderData;
	ASSERT_TRUE(bitStream.Read(hasBorderData));
	EXPECT_TRUE(hasBorderData);
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