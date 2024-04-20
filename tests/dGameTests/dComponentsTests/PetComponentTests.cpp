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

/**
 * Test bitset pet flags
*/
TEST_F(PetTest, PetComponentFlagTest) {
	using enum PetFlag;

	// Test setting and reading single flags, exclusively
	petComponent->m_Flags.Reset(NONE);
	ASSERT_TRUE(petComponent->m_Flags.HasOnly(NONE));
	petComponent->m_Flags.Reset(TAMEABLE);
	ASSERT_TRUE(petComponent->m_Flags.HasOnly(TAMEABLE));
	ASSERT_FALSE(petComponent->m_Flags.HasOnly(SPAWNING));

	// Test setting and reading multiple flags, exclusively
	petComponent->m_Flags.Reset(NOT_WAITING, SPAWNING);
	ASSERT_FALSE(petComponent->m_Flags.Has(TAMEABLE));
	ASSERT_TRUE(petComponent->m_Flags.Has(NOT_WAITING));
	ASSERT_TRUE(petComponent->m_Flags.Has(SPAWNING));
	ASSERT_TRUE(petComponent->m_Flags.Has(NOT_WAITING, SPAWNING));
	ASSERT_FALSE(petComponent->m_Flags.HasOnly(NOT_WAITING));
	ASSERT_FALSE(petComponent->m_Flags.HasOnly(SPAWNING));
	ASSERT_TRUE(petComponent->m_Flags.HasOnly(NOT_WAITING, SPAWNING));

	// Test flags are being properly reset for next batch of tests
	petComponent->m_Flags.Reset(NONE);
	ASSERT_TRUE(petComponent->m_Flags.Has(NONE));
	ASSERT_TRUE(petComponent->m_Flags.HasOnly(NONE));
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING));
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING, SPAWNING, TAMEABLE));

	// Test setting and reading single flags, non-exclusively
	petComponent->m_Flags.Set(NOT_WAITING);
	ASSERT_TRUE(petComponent->m_Flags.Has(NOT_WAITING));
	ASSERT_FALSE(petComponent->m_Flags.Has(SPAWNING));

	// Test setting and reading multiple flags, non-exclusively
	petComponent->m_Flags.Set(TAMEABLE, BEING_TAMED);
	ASSERT_TRUE(petComponent->m_Flags.Has(TAMEABLE, BEING_TAMED));
	ASSERT_TRUE(petComponent->m_Flags.Has(NOT_WAITING));
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING, SPAWNING));
	ASSERT_TRUE(petComponent->m_Flags.Has(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->m_Flags.Has(SPAWNING));
	ASSERT_FALSE(petComponent->m_Flags.Has(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));

	// Test unsetting and reading multiple flags, non-exclusively
	petComponent->m_Flags.Unset(NOT_WAITING, SPAWNING);
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING, SPAWNING));
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING));
	ASSERT_TRUE(petComponent->m_Flags.Has(TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->m_Flags.Has(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->m_Flags.Has(SPAWNING));
	ASSERT_FALSE(petComponent->m_Flags.Has(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));
}

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
