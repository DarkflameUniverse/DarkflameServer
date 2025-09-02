#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "PetComponent.h"
#include "Entity.h"
#include "eReplicaComponentType.h"
#include "ePetAbilityType.h"
#include "eStateChangeType.h"

class PetComponentTest : public GameDependenciesTest {
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

TEST_F(PetComponentTest, PlacementNewAddComponentTest) {
	// Test adding component
	ASSERT_NE(petComponent, nullptr);
	baseEntity->AddComponent<PetComponent>(1);
	ASSERT_NE(baseEntity->GetComponent<PetComponent>(), nullptr);

	// Test getting initial status
	ASSERT_EQ(petComponent->GetParent()->GetObjectID(), 15);
	ASSERT_EQ(petComponent->GetAbility(), ePetAbilityType::Invalid);
}

// Test untamed pet serialization (initial update)
TEST_F(PetComponentTest, UntamedPetInitialSerialization) {
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
	EXPECT_EQ(status, 67108866); // Default status should be 67108866 since that is the untamed state

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

// Test pet with interaction serialization
TEST_F(PetComponentTest, PetWithInteractionSerialization) {
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
