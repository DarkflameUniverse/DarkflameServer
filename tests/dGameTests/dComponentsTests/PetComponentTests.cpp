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
	using enum PetFlag;

	// Test adding component
	ASSERT_NE(petComponent, nullptr);
	baseEntity->AddComponent<PetComponent>(1);
	ASSERT_NE(baseEntity->GetComponent<PetComponent>(), nullptr);

	// Test getting initial status
	ASSERT_EQ(petComponent->GetParent()->GetObjectID(), 15);
	ASSERT_EQ(petComponent->GetPetAiState(), PetAiState::spawn);
}


TEST_F(PetTest, PetAiState) {
	const auto initialState = petComponent->GetPetAiState();
	ASSERT_EQ(initialState, PetAiState::spawn);

	petComponent->SetPetAiState(PetAiState::follow);
	ASSERT_EQ(PetAiState::follow, petComponent->GetPetAiState());

	petComponent->SetPetAiState(PetAiState::idle);
	ASSERT_EQ(PetAiState::idle, petComponent->GetPetAiState());

	petComponent->SetPetAiState(PetAiState::interact);
	ASSERT_EQ(PetAiState::interact, petComponent->GetPetAiState());

	petComponent->SetPetAiState(PetAiState::despawn);
	ASSERT_EQ(PetAiState::despawn, petComponent->GetPetAiState());
}

// Test the pet use logic
TEST_F(PetTest, PetUse) {
	ASSERT_FALSE(petComponent->IsReadyToInteract());

	petComponent->SetIsReadyToInteract(true);
	ASSERT_TRUE(petComponent->IsReadyToInteract());

	// Test bouncer logic
	ASSERT_FALSE(petComponent->IsHandlingInteraction());
	petComponent->OnUse(baseEntity);

	// need to add a destroyable component to the test entity and test the imagination drain

}
