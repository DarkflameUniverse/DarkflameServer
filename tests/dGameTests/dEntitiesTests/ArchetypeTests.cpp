#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Archetype.h"

class ArchetypeTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	Entity* newEntity;

	std::unique_ptr<Archetype<DestroyableComponent, SimplePhysicsComponent>> baseArchetype;
	
	CBITSTREAM

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		newEntity = new Entity(16, GameDependenciesTest::info);

		baseArchetype = std::make_unique<Archetype<DestroyableComponent, SimplePhysicsComponent>>(0);
	}

	void TearDown() override {
		delete baseEntity;
		delete newEntity;
		TearDownDependencies();
	}
};

TEST_F(ArchetypeTest, PlacementNewAddArchetypeTest) {
	ASSERT_NE(baseEntity, nullptr);
	ASSERT_NE(baseArchetype, nullptr);

	// Create a destroyable component and simple physics component and test their parent entity was assigned correctly
	baseArchetype->CreateComponents(DestroyableComponent(baseEntity), SimplePhysicsComponent(baseEntity, 2));
	
	auto& destroyableComponent = baseArchetype->GetComponent<DestroyableComponent>(0);
	ASSERT_EQ(destroyableComponent.GetParent(), baseEntity);
	ASSERT_NO_THROW(destroyableComponent.GetArmor());

	auto& simplePhysicsComponent = baseArchetype->GetComponent<SimplePhysicsComponent>(0);
	ASSERT_EQ(simplePhysicsComponent.GetParent(), baseEntity);

	// Set and read some values
	const NiPoint3 testPos{-1.0, 1.0, 3.0};
	ASSERT_NO_THROW(simplePhysicsComponent.SetPosition(testPos));
	ASSERT_EQ(simplePhysicsComponent.GetPosition(), testPos);

	// Add another entry to the archetype
	baseArchetype->CreateComponents(DestroyableComponent(newEntity), SimplePhysicsComponent(newEntity, 2));

	auto& newDestroyableComponent = baseArchetype->GetComponent<DestroyableComponent>(1);
	ASSERT_EQ(newDestroyableComponent.GetParent(), newEntity);
	ASSERT_NE(newDestroyableComponent.GetParent(), baseEntity);

	auto& newSimplePhysicsComponent = baseArchetype->GetComponent<SimplePhysicsComponent>(1);
	ASSERT_EQ(newSimplePhysicsComponent.GetParent(), newEntity);
	ASSERT_NE(newSimplePhysicsComponent.GetParent(), baseEntity);

	// Try deleting elements
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	ASSERT_ANY_THROW(baseArchetype->DeleteComponents(0));
}
