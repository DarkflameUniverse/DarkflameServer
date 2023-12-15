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
		petComponent->SetStatus(0);
		petComponent->SetPetAiState(PetAiState::spawn);
		petComponent->SetAbility(ePetAbilityType::Invalid);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

TEST_F(PetTest, PlacementNewAddComponentTest) {
	ASSERT_NE(petComponent, nullptr);
	baseEntity->AddComponent<PetComponent>(1);
	ASSERT_NE(baseEntity->GetComponent<PetComponent>(), nullptr);
	ASSERT_EQ(petComponent->GetParent()->GetObjectID(), 15);
	ASSERT_EQ(petComponent->GetStatus(), PetFlag::NONE);
	ASSERT_EQ(petComponent->GetPetAiState(), PetAiState::spawn);
	ASSERT_EQ(petComponent->GetAbility(), ePetAbilityType::Invalid);
}

/**
 * Test bitset pet flags
*/
TEST_F(PetTest, PetComponentFlagTest) {
	// Test setting and reading single flags
	petComponent->SetFlag(NOT_WAITING);
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));

	// Test setting and reading multiple flags
	petComponent->SetFlag(TAMEABLE, BEING_TAMED);
	ASSERT_TRUE(petComponent->HasFlag(TAMEABLE, BEING_TAMED));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, SPAWNING));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));

	// Test unsetting and reading multiple flags
	petComponent->UnsetFlag(NOT_WAITING, SPAWNING);
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_TRUE(petComponent->HasFlag(TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));
}
