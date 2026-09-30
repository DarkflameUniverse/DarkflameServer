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
	RakNet::BitStream bitStream;

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

// The client reads the names bit on every update while the pet is dirty (as live wrote it); DLU wrote it only on
// construction, so the client took the next component's first bit for it
TEST_F(PetTest, UpdateWritesTheNamesBit) {
	petComponent->Serialize(bitStream, false);
	// dirty, status, ability, interaction, owner, names
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 1u + 32 + 32 + 1 + 1 + 1);
	bitStream.IgnoreBits(1 + 32 + 32 + 1 + 1);
	bool names = true;
	ASSERT_TRUE(bitStream.Read(names));
	EXPECT_FALSE(names);
}
