#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "DonationVendorComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class DonationVendorComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	DonationVendorComponent* donationVendorComponent;
	CBITSTREAM
	uint32_t flags = 0;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		// Set a custom goal for testing
		baseEntity->SetVar<int32_t>(u"donationGoal", 1000);
		baseEntity->SetVar<uint32_t>(u"activityID", 123);
		donationVendorComponent = baseEntity->AddComponent<DonationVendorComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Test initial serialization with no donations
TEST_F(DonationVendorComponentTest, InitialSerialization) {
	donationVendorComponent->Serialize(bitStream, true);
	
	// Read VendorComponent serialization first
	bool vendorDirtyFlag;
	bitStream.Read(vendorDirtyFlag);
	EXPECT_TRUE(vendorDirtyFlag); // Should be true for initial update
	
	if (vendorDirtyFlag) {
		bool hasStandardCostItems;
		bool hasMultiCostItems;
		bitStream.Read(hasStandardCostItems);
		bitStream.Read(hasMultiCostItems);
		EXPECT_FALSE(hasStandardCostItems);
		EXPECT_FALSE(hasMultiCostItems);
	}
	
	// Read DonationVendorComponent serialization
	bool donationDirtyFlag;
	bitStream.Read(donationDirtyFlag);
	EXPECT_TRUE(donationDirtyFlag); // Should be true for initial update
	
	if (donationDirtyFlag) {
		float percentComplete;
		int32_t totalDonated;
		int32_t totalRemaining;
		
		bitStream.Read(percentComplete);
		bitStream.Read(totalDonated);
		bitStream.Read(totalRemaining);
		
		EXPECT_EQ(percentComplete, 0.0f);
		EXPECT_EQ(totalDonated, 0);
		EXPECT_EQ(totalRemaining, 1000);
	}
	
	bitStream.Reset();
}

// Test serialization after donations
TEST_F(DonationVendorComponentTest, SerializationAfterDonations) {
	// Submit some donations
	donationVendorComponent->SubmitDonation(250);
	
	donationVendorComponent->Serialize(bitStream, false);
	
	// Read VendorComponent serialization first
	bool vendorDirtyFlag;
	bitStream.Read(vendorDirtyFlag);
	EXPECT_FALSE(vendorDirtyFlag); // Should be false for regular update
	
	// Read DonationVendorComponent serialization
	bool donationDirtyFlag;
	bitStream.Read(donationDirtyFlag);
	EXPECT_TRUE(donationDirtyFlag); // Should be true because we submitted donations
	
	if (donationDirtyFlag) {
		float percentComplete;
		int32_t totalDonated;
		int32_t totalRemaining;
		
		bitStream.Read(percentComplete);
		bitStream.Read(totalDonated);
		bitStream.Read(totalRemaining);
		
		EXPECT_EQ(percentComplete, 0.25f); // 250/1000 = 0.25
		EXPECT_EQ(totalDonated, 250);
		EXPECT_EQ(totalRemaining, 750);
	}
	
	bitStream.Reset();
}

// Test default jawbox activity (ID 117)
TEST_F(DonationVendorComponentTest, JawboxActivitySerialization) {
	delete baseEntity;
	baseEntity = new Entity(15, GameDependenciesTest::info);
	baseEntity->SetVar<uint32_t>(u"activityID", 117); // Jawbox activity
	donationVendorComponent = baseEntity->AddComponent<DonationVendorComponent>();
	
	donationVendorComponent->Serialize(bitStream, true);
	
	// Read VendorComponent serialization first
	bool vendorDirtyFlag;
	bitStream.Read(vendorDirtyFlag);
	EXPECT_TRUE(vendorDirtyFlag);
	
	if (vendorDirtyFlag) {
		bool hasStandardCostItems;
		bool hasMultiCostItems;
		bitStream.Read(hasStandardCostItems);
		bitStream.Read(hasMultiCostItems);
	}
	
	// Read DonationVendorComponent serialization
	bool donationDirtyFlag;
	bitStream.Read(donationDirtyFlag);
	EXPECT_TRUE(donationDirtyFlag);
	
	if (donationDirtyFlag) {
		float percentComplete;
		int32_t totalDonated;
		int32_t totalRemaining;
		
		bitStream.Read(percentComplete);
		bitStream.Read(totalDonated);
		bitStream.Read(totalRemaining);
		
		// Jawbox activity should show as complete
		EXPECT_EQ(percentComplete, 1.0f);
		EXPECT_EQ(totalDonated, INT32_MAX);
		EXPECT_EQ(totalRemaining, 0);
	}
	
	bitStream.Reset();
}

// Test regular update without changes (dirty flag should be false)
TEST_F(DonationVendorComponentTest, RegularUpdateWithoutChanges) {
	// First do an initial update to clear the dirty flag
	donationVendorComponent->Serialize(bitStream, true);
	bitStream.Reset();
	
	// Now do a regular update without any changes
	donationVendorComponent->Serialize(bitStream, false);
	
	// Read VendorComponent serialization first
	bool vendorDirtyFlag;
	bitStream.Read(vendorDirtyFlag);
	EXPECT_FALSE(vendorDirtyFlag);
	
	// Read DonationVendorComponent serialization
	bool donationDirtyFlag;
	bitStream.Read(donationDirtyFlag);
	EXPECT_FALSE(donationDirtyFlag); // Should be false since nothing changed
	
	bitStream.Reset();
}