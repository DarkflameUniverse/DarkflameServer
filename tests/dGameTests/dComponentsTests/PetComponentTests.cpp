#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "PetComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "ePetAbilityType.h"
#include "eStateChangeType.h"

class PetTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	PetComponent* petComponent;
	CBITSTREAM

	void SetUp() override {
		SetUpDependencies();

		// Set up entity and pet component
		baseEntity = new Entity(15, GameDependenciesTest::info);
		petComponent = baseEntity->AddComponent<PetComponent>(1);

		// Initialize some values to be not default

	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

TEST_F(PetTest, PlacementNewAddComponentTest) {
	// Test adding component
	ASSERT_NE(petComponent, nullptr);
	baseEntity->AddComponent<PetComponent>(1);
	ASSERT_NE(baseEntity->GetComponent<PetComponent>(), nullptr);

	// Test getting initial status
	ASSERT_EQ(petComponent->GetParent()->GetObjectID(), 15);
	ASSERT_EQ(petComponent->GetAbility(), ePetAbilityType::Invalid);
}

// Test untamed pet serialization (initial update)
TEST_F(PetTest, UntamedPetInitialSerialization) {
	petComponent->Serialize(bitStream, true);
	
	// Read the serialized data manually
	bool alwaysDirty;
	uint32_t status;
	ePetAbilityType ability;
	bool interacting;
	bool tamed;
	bool tamedForInitial;
	
	bitStream.Read(alwaysDirty);
	EXPECT_TRUE(alwaysDirty); // Always true
	
	bitStream.Read(status);
	EXPECT_EQ(status, 0); // Default status should be 0
	
	bitStream.Read(ability);
	EXPECT_EQ(ability, ePetAbilityType::Invalid); // Should be Invalid for untamed pets
	
	bitStream.Read(interacting);
	EXPECT_FALSE(interacting); // No interaction by default
	
	bitStream.Read(tamed);
	EXPECT_FALSE(tamed); // Pet is not tamed by default
	
	// For initial update, should write tamed flag again
	bitStream.Read(tamedForInitial);
	EXPECT_FALSE(tamedForInitial); // Should match tamed flag
	
	bitStream.Reset();
}

// Test tamed pet serialization (initial update)
TEST_F(PetTest, TamedPetInitialSerialization) {
	// Set up a tamed pet
	LWOOBJID ownerID = 12345;
	petComponent->Activate(ownerID, false, false);
	petComponent->SetPetNameForModeration("TestPet");
	petComponent->SetOwnerName("TestOwner");
	
	petComponent->Serialize(bitStream, true);
	
	// Read the serialized data manually
	bool alwaysDirty;
	uint32_t status;
	ePetAbilityType ability;
	bool interacting;
	bool tamed;
	LWOOBJID owner;
	bool tamedForInitial;
	uint32_t moderationStatus;
	uint8_t nameLength;
	std::vector<uint16_t> nameData;
	uint8_t ownerNameLength;
	std::vector<uint16_t> ownerNameData;
	
	bitStream.Read(alwaysDirty);
	EXPECT_TRUE(alwaysDirty); // Always true
	
	bitStream.Read(status);
	bitStream.Read(ability);
	EXPECT_NE(ability, ePetAbilityType::Invalid); // Should have a valid ability when tamed
	
	bitStream.Read(interacting);
	EXPECT_FALSE(interacting); // No interaction by default
	
	bitStream.Read(tamed);
	EXPECT_TRUE(tamed); // Pet should be tamed
	
	if (tamed) {
		bitStream.Read(owner);
		EXPECT_EQ(owner, ownerID);
	}
	
	// For initial update with tamed pet
	bitStream.Read(tamedForInitial);
	EXPECT_TRUE(tamedForInitial);
	
	if (tamedForInitial) {
		bitStream.Read(moderationStatus);
		EXPECT_EQ(moderationStatus, 0); // Default moderation status
		
		bitStream.Read(nameLength);
		EXPECT_GT(nameLength, 0); // Should have a name
		
		nameData.resize(nameLength);
		for (uint8_t i = 0; i < nameLength; i++) {
			bitStream.Read(nameData[i]);
		}
		
		bitStream.Read(ownerNameLength);
		EXPECT_GT(ownerNameLength, 0); // Should have an owner name
		
		ownerNameData.resize(ownerNameLength);
		for (uint8_t i = 0; i < ownerNameLength; i++) {
			bitStream.Read(ownerNameData[i]);
		}
	}
	
	bitStream.Reset();
}

// Test tamed pet regular update serialization
TEST_F(PetTest, TamedPetRegularSerialization) {
	// Set up a tamed pet
	LWOOBJID ownerID = 12345;
	petComponent->Activate(ownerID, false, false);
	
	petComponent->Serialize(bitStream, false);
	
	// Read the serialized data manually
	bool alwaysDirty;
	uint32_t status;
	ePetAbilityType ability;
	bool interacting;
	bool tamed;
	LWOOBJID owner;
	
	bitStream.Read(alwaysDirty);
	EXPECT_TRUE(alwaysDirty); // Always true
	
	bitStream.Read(status);
	bitStream.Read(ability);
	EXPECT_NE(ability, ePetAbilityType::Invalid); // Should have a valid ability when tamed
	
	bitStream.Read(interacting);
	EXPECT_FALSE(interacting); // No interaction by default
	
	bitStream.Read(tamed);
	EXPECT_TRUE(tamed); // Pet should be tamed
	
	if (tamed) {
		bitStream.Read(owner);
		EXPECT_EQ(owner, ownerID);
	}
	
	// Regular update should not include initial update data
	EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
	
	bitStream.Reset();
}

// Test pet with interaction serialization
TEST_F(PetTest, PetWithInteractionSerialization) {
	// Set up a pet with interaction
	LWOOBJID interactionID = 67890;
	petComponent->SetInteraction(interactionID);
	
	petComponent->Serialize(bitStream, false);
	
	// Read the serialized data manually
	bool alwaysDirty;
	uint32_t status;
	ePetAbilityType ability;
	bool interacting;
	LWOOBJID interaction;
	bool tamed;
	
	bitStream.Read(alwaysDirty);
	EXPECT_TRUE(alwaysDirty); // Always true
	
	bitStream.Read(status);
	bitStream.Read(ability);
	EXPECT_EQ(ability, ePetAbilityType::Invalid); // Should be Invalid for untamed pets
	
	bitStream.Read(interacting);
	EXPECT_TRUE(interacting); // Should be true
	
	if (interacting) {
		bitStream.Read(interaction);
		EXPECT_EQ(interaction, interactionID);
	}
	
	bitStream.Read(tamed);
	EXPECT_FALSE(tamed); // Pet is not tamed by default
	
	bitStream.Reset();
}
