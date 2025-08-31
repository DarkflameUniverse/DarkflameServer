#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "VendorComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

class VendorComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	VendorComponent* vendorComponent;
	CBITSTREAM;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		vendorComponent = baseEntity->AddComponent<VendorComponent>();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

TEST_F(VendorComponentTest, VendorComponentSerializeInitialTest) {
	vendorComponent->Serialize(bitStream, true);
	
	// Should always write true for initial update
	bool hasVendorUpdate;
	bitStream.Read(hasVendorUpdate);
	ASSERT_TRUE(hasVendorUpdate);
	
	// Should write standard and multi cost flags
	bool hasStandardCostItems, hasMultiCostItems;
	bitStream.Read(hasStandardCostItems);
	bitStream.Read(hasMultiCostItems);
	
	// Default values should be false
	ASSERT_FALSE(hasStandardCostItems);
	ASSERT_FALSE(hasMultiCostItems);
}

TEST_F(VendorComponentTest, VendorComponentSerializeUpdateTest) {
	// Test non-initial update without dirty flag
	vendorComponent->Serialize(bitStream, false);
	
	bool hasVendorUpdate;
	bitStream.Read(hasVendorUpdate);
	ASSERT_FALSE(hasVendorUpdate);  // Not dirty, should be false
}

TEST_F(VendorComponentTest, VendorComponentDirtyFlagTest) {
	// Set some values to make it dirty
	vendorComponent->SetHasStandardCostItems(true);
	vendorComponent->SetHasMultiCostItems(true);
	
	vendorComponent->Serialize(bitStream, false);
	
	bool hasVendorUpdate;
	bitStream.Read(hasVendorUpdate);
	ASSERT_TRUE(hasVendorUpdate);  // Should be dirty and have update
	
	bool hasStandardCostItems, hasMultiCostItems;
	bitStream.Read(hasStandardCostItems);
	bitStream.Read(hasMultiCostItems);
	
	ASSERT_TRUE(hasStandardCostItems);
	ASSERT_TRUE(hasMultiCostItems);
}



TEST_F(VendorComponentTest, VendorComponentCostItemsTest) {
	// Test setting cost items flags - these are private members
	// We can only test the serialization behavior
	vendorComponent->SetHasStandardCostItems(true);
	vendorComponent->SetHasMultiCostItems(true);
	
	// Test serialization with flags set
	vendorComponent->Serialize(bitStream, true);
	
	bool hasVendorUpdate;
	bitStream.Read(hasVendorUpdate);
	ASSERT_TRUE(hasVendorUpdate);
	
	bool hasStandardCostItems, hasMultiCostItems;
	bitStream.Read(hasStandardCostItems);
	bitStream.Read(hasMultiCostItems);
	
	ASSERT_TRUE(hasStandardCostItems);
	ASSERT_TRUE(hasMultiCostItems);
}