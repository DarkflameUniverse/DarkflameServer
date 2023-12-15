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
	ASSERT_TRUE(petComponent->HasFlag(NONE));
	ASSERT_EQ(petComponent->GetPetAiState(), PetAiState::spawn);
	ASSERT_EQ(petComponent->GetAbility(), ePetAbilityType::Invalid);
}

/**
 * Test bitset pet flags
*/
TEST_F(PetTest, PetComponentFlagTest) {
	// Test setting and reading single flags, exclusively
	petComponent->SetOnlyFlag(NONE);
	ASSERT_TRUE(petComponent->HasOnlyFlag(NONE));
	petComponent->SetOnlyFlag(TAMEABLE);
	ASSERT_TRUE(petComponent->HasOnlyFlag(TAMEABLE));
	ASSERT_FALSE(petComponent->HasOnlyFlag(SPAWNING));

	// Test setting and reading multiple flags, exclusively
	petComponent->SetOnlyFlag(NOT_WAITING, SPAWNING);
	ASSERT_FALSE(petComponent->HasFlag(TAMEABLE));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_TRUE(petComponent->HasFlag(SPAWNING));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING, SPAWNING));
	ASSERT_FALSE(petComponent->HasOnlyFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasOnlyFlag(SPAWNING));
	ASSERT_TRUE(petComponent->HasOnlyFlag(NOT_WAITING, SPAWNING));

	// Test flags are being properly reset for next batch of tests
	petComponent->SetOnlyFlag(NONE);
	ASSERT_TRUE(petComponent->HasFlag(NONE));
	ASSERT_TRUE(petComponent->HasOnlyFlag(NONE));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, SPAWNING, TAMEABLE));

	// Test setting and reading single flags, non-exclusively
	petComponent->SetFlag(NOT_WAITING);
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));

	// Test setting and reading multiple flags, non-exclusively
	petComponent->SetFlag(TAMEABLE, BEING_TAMED);
	ASSERT_TRUE(petComponent->HasFlag(TAMEABLE, BEING_TAMED));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, SPAWNING));
	ASSERT_TRUE(petComponent->HasFlag(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));

	// Test unsetting and reading multiple flags, non-exclusively
	petComponent->UnsetFlag(NOT_WAITING, SPAWNING);
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING));
	ASSERT_TRUE(petComponent->HasFlag(TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(NOT_WAITING, TAMEABLE, BEING_TAMED));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING));
	ASSERT_FALSE(petComponent->HasFlag(SPAWNING, NOT_WAITING, TAMEABLE, BEING_TAMED));
}
