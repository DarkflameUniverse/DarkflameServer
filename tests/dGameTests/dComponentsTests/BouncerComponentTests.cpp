#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "BouncerComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"

class BouncerTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	BouncerComponent* bouncerComponent;
	CBITSTREAM
	uint32_t flags = 0;
	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		// BouncerComponent constructor doesn't require parameters
		bouncerComponent = baseEntity->AddComponent<BouncerComponent>();
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

/**
 * Test serialization of a BouncerComponent with pet enabled false
 */
TEST_F(BouncerTest, SerializePetDisabledTest) {
	bitStream.Reset();
	
	// Default state should have pet disabled
	bouncerComponent->SetPetEnabled(false);
	
	// Now we test a serialization for correctness.
	bouncerComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool petEnabled;
	bitStream.Read(petEnabled);
	EXPECT_EQ(petEnabled, false);
	
	// When pet is disabled, there should be no additional data
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

/**
 * Test serialization of a BouncerComponent with pet enabled true
 */
TEST_F(BouncerTest, SerializePetEnabledTest) {
	bitStream.Reset();
	
	// Enable pet and set bouncer state
	bouncerComponent->SetPetEnabled(true);
	bouncerComponent->SetPetBouncerEnabled(true);
	
	// Now we test a serialization for correctness.
	bouncerComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool petEnabled;
	bitStream.Read(petEnabled);
	EXPECT_EQ(petEnabled, true);
	
	bool petBouncerEnabled;
	bitStream.Read(petBouncerEnabled);
	EXPECT_EQ(petBouncerEnabled, true);
}

/**
 * Test serialization of a BouncerComponent with pet enabled but bouncer disabled
 */
TEST_F(BouncerTest, SerializePetEnabledBouncerDisabledTest) {
	bitStream.Reset();
	
	// Enable pet but disable bouncer
	bouncerComponent->SetPetEnabled(true);
	bouncerComponent->SetPetBouncerEnabled(false);
	
	// Now we test a serialization for correctness.
	bouncerComponent->Serialize(bitStream, false);
	
	// Read back the serialized data
	bool petEnabled;
	bitStream.Read(petEnabled);
	EXPECT_EQ(petEnabled, true);
	
	bool petBouncerEnabled;
	bitStream.Read(petBouncerEnabled);
	EXPECT_EQ(petBouncerEnabled, false);
}

/**
 * Test serialization during initial update
 */
TEST_F(BouncerTest, SerializeInitialUpdateTest) {
	bitStream.Reset();
	
	// Enable pet and set bouncer state
	bouncerComponent->SetPetEnabled(true);
	bouncerComponent->SetPetBouncerEnabled(true);
	
	// Now we test a serialization for correctness with initial update.
	bouncerComponent->Serialize(bitStream, true);
	
	// Read back the serialized data - behavior should be same as regular update
	bool petEnabled;
	bitStream.Read(petEnabled);
	EXPECT_EQ(petEnabled, true);
	
	bool petBouncerEnabled;
	bitStream.Read(petBouncerEnabled);
	EXPECT_EQ(petBouncerEnabled, true);
}
