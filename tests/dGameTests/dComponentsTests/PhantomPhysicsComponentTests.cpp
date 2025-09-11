#include <gtest/gtest.h>

#include "PhantomPhysicsComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class PhantomPhysicsComponentTest : public GameDependenciesTest {
protected:
};

/**
 * Test PhantomPhysicsComponent serialization for initial update
 */
TEST_F(PhantomPhysicsComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	PhantomPhysicsComponent phantomPhysicsComponent(&testEntity, 1); // Need componentId parameter
	
	RakNet::BitStream bitStream;
	phantomPhysicsComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	// PhantomPhysicsComponent writes physics data (from PhysicsComponent)
	bool hasPhysicsData;
	ASSERT_TRUE(bitStream.Read(hasPhysicsData));
	EXPECT_TRUE(hasPhysicsData); // Always true for initial update
	
	if (hasPhysicsData) {
		// Position data (x, y, z)
		float x, y, z;
		ASSERT_TRUE(bitStream.Read(x));
		ASSERT_TRUE(bitStream.Read(y));
		ASSERT_TRUE(bitStream.Read(z));
		EXPECT_EQ(x, 0.0f); // Default position
		EXPECT_EQ(y, 0.0f);
		EXPECT_EQ(z, 0.0f);
		
		// Rotation data (x, y, z, w) - note: not w first like NiQuaternion
		float rX, rY, rZ, rW;
		ASSERT_TRUE(bitStream.Read(rX));
		ASSERT_TRUE(bitStream.Read(rY));
		ASSERT_TRUE(bitStream.Read(rZ));
		ASSERT_TRUE(bitStream.Read(rW));
		EXPECT_EQ(rX, 0.0f); // Default rotation
		EXPECT_EQ(rY, 0.0f);
		EXPECT_EQ(rZ, 0.0f);
		EXPECT_EQ(rW, 0.0f); // Default quaternion (not identity)
	}
}

/**
 * Test PhantomPhysicsComponent serialization for regular update (no changes)
 */
TEST_F(PhantomPhysicsComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	PhantomPhysicsComponent phantomPhysicsComponent(&testEntity, 1); // Need componentId parameter
	
	// Reset dirty flags with initial serialization
	RakNet::BitStream initStream;
	phantomPhysicsComponent.Serialize(initStream, true);
	
	RakNet::BitStream bitStream;
	phantomPhysicsComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	// For regular updates with no dirty flags, should write false
	bool hasPhysicsData;
	ASSERT_TRUE(bitStream.Read(hasPhysicsData));
	EXPECT_FALSE(hasPhysicsData); // No changes
}