#include <gtest/gtest.h>

#include "VendorComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "GameDependencies.h"

class VendorComponentTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
	}
	
	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(VendorComponentTest, SerializeInitialUpdate) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	RakNet::BitStream bitStream;
	vendorComponent.Serialize(bitStream, true);
	
	// Read the data manually to validate serialization format
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo); // Should always be true for initial update
	
	bool hasStandardCostItems;
	ASSERT_TRUE(bitStream.Read(hasStandardCostItems));
	EXPECT_FALSE(hasStandardCostItems); // Default state after RefreshInventory
	
	bool hasMultiCostItems;
	ASSERT_TRUE(bitStream.Read(hasMultiCostItems));
	EXPECT_FALSE(hasMultiCostItems); // Default state
}

TEST_F(VendorComponentTest, SerializeRegularUpdate) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	// Do initial serialization to populate data 
	RakNet::BitStream initStream;
	vendorComponent.Serialize(initStream, true);
	
	// Do a regular update to clear dirty flag
	RakNet::BitStream clearStream;
	vendorComponent.Serialize(clearStream, false);
	
	// Now test regular update with no changes
	RakNet::BitStream bitStream;
	vendorComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_FALSE(hasVendorInfo); // No dirty flags after clearing, so no data
}

TEST_F(VendorComponentTest, SerializeWithDirtyVendor) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	// Reset dirty flag
	RakNet::BitStream initStream;
	vendorComponent.Serialize(initStream, true);
	
	// Make vendor dirty by changing state
	vendorComponent.SetHasStandardCostItems(true);
	
	RakNet::BitStream bitStream;
	vendorComponent.Serialize(bitStream, false);
	
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo); // Should be true due to dirty flag
	
	bool hasStandardCostItems;
	ASSERT_TRUE(bitStream.Read(hasStandardCostItems));
	EXPECT_TRUE(hasStandardCostItems); // Changed to true
	
	bool hasMultiCostItems;
	ASSERT_TRUE(bitStream.Read(hasMultiCostItems));
	EXPECT_FALSE(hasMultiCostItems); // Still false
}

TEST_F(VendorComponentTest, SerializeWithMultiCostItems) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	// Set both flags
	vendorComponent.SetHasStandardCostItems(true);
	vendorComponent.SetHasMultiCostItems(true);
	
	RakNet::BitStream bitStream;
	vendorComponent.Serialize(bitStream, true);
	
	bitStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(bitStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo);
	
	bool hasStandardCostItems;
	ASSERT_TRUE(bitStream.Read(hasStandardCostItems));
	EXPECT_TRUE(hasStandardCostItems);
	
	bool hasMultiCostItems;
	ASSERT_TRUE(bitStream.Read(hasMultiCostItems));
	EXPECT_TRUE(hasMultiCostItems);
}

TEST_F(VendorComponentTest, SerializeDirtyFlagClearing) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	// Make vendor dirty
	vendorComponent.SetHasStandardCostItems(true);
	
	// First serialize should clear dirty flag
	RakNet::BitStream firstStream;
	vendorComponent.Serialize(firstStream, false);
	
	// Second serialize should show no vendor info
	RakNet::BitStream secondStream;
	vendorComponent.Serialize(secondStream, false);
	
	secondStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(secondStream.Read(hasVendorInfo));
	EXPECT_FALSE(hasVendorInfo); // Dirty flag should be cleared
}

TEST_F(VendorComponentTest, SettersChangeDirtyFlag) {
	Entity testEntity(15, info);
	VendorComponent vendorComponent(&testEntity);
	
	// Clear initial dirty state
	RakNet::BitStream initStream;
	vendorComponent.Serialize(initStream, true);
	
	// Setting same value should not make dirty
	vendorComponent.SetHasStandardCostItems(false); // Already false
	
	RakNet::BitStream noChangeStream;
	vendorComponent.Serialize(noChangeStream, false);
	noChangeStream.ResetReadPointer();
	
	bool hasVendorInfo;
	ASSERT_TRUE(noChangeStream.Read(hasVendorInfo));
	EXPECT_FALSE(hasVendorInfo); // Should not be dirty
	
	// Setting different value should make dirty
	vendorComponent.SetHasStandardCostItems(true);
	
	RakNet::BitStream changeStream;
	vendorComponent.Serialize(changeStream, false);
	changeStream.ResetReadPointer();
	
	ASSERT_TRUE(changeStream.Read(hasVendorInfo));
	EXPECT_TRUE(hasVendorInfo); // Should be dirty now
}