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
