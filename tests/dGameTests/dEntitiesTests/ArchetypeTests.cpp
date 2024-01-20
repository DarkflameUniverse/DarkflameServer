#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <typeindex>
#include <unordered_set>

#include "Logger.h"
#include "Game.h"
#include "BitStream.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"
#include "Archetype.h"
#include "EntitySystem.h"
#include "ObjectIDManager.h"

class ArchetypeTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> baseEntity;
	std::unique_ptr<Entity> newEntity;
	std::unique_ptr<Archetype<DestroyableComponent, SimplePhysicsComponent>> baseArchetype;

	std::unique_ptr<Logger> logger = std::make_unique<Logger>("./ArchetypeTests", true, true);

	CBITSTREAM
		void SetUp() override {
		SetUpDependencies();

		Game::logger = logger.get();

		baseEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		newEntity = std::make_unique<Entity>(16, GameDependenciesTest::info);
		baseArchetype = std::make_unique<Archetype<DestroyableComponent, SimplePhysicsComponent>>(0);
	}

	void TearDown() override {
		Game::logger = nullptr;

		TearDownDependencies();
	}
};

TEST_F(ArchetypeTest, PlacementNewAddArchetypeTest) {
	ASSERT_NE(baseEntity.get(), nullptr);
	ASSERT_NE(baseArchetype.get(), nullptr);

	// Create a destroyable component and simple physics component and test their parent entity was assigned correctly
	const auto baseEntityId = baseEntity->GetObjectID();
	baseArchetype->CreateComponents(DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));

	auto& destroyableComponent = baseArchetype->GetComponent<DestroyableComponent>(0);
	ASSERT_EQ(destroyableComponent.GetParent(), baseEntity.get());
	ASSERT_NO_THROW(destroyableComponent.GetArmor());

	auto& simplePhysicsComponent = baseArchetype->GetComponent<SimplePhysicsComponent>(0);
	ASSERT_EQ(simplePhysicsComponent.GetParent(), baseEntity.get());

	// Set and read some values
	const NiPoint3 testPos{ -1.0, 1.0, 3.0 };
	ASSERT_NO_THROW(simplePhysicsComponent.SetPosition(testPos));
	ASSERT_EQ(simplePhysicsComponent.GetPosition(), testPos);

	// Add another entry to the archetype
	const auto newEntityId = newEntity->GetObjectID();
	baseArchetype->CreateComponents(DestroyableComponent(newEntityId), SimplePhysicsComponent(newEntityId, 2));

	auto& newDestroyableComponent = baseArchetype->GetComponent<DestroyableComponent>(1);
	ASSERT_EQ(newDestroyableComponent.GetParent(), newEntity.get());
	ASSERT_NE(newDestroyableComponent.GetParent(), baseEntity.get());

	auto& newSimplePhysicsComponent = baseArchetype->GetComponent<SimplePhysicsComponent>(1);
	ASSERT_EQ(newSimplePhysicsComponent.GetParent(), newEntity.get());
	ASSERT_NE(newSimplePhysicsComponent.GetParent(), baseEntity.get());

	size_t destCompSize = sizeof(baseArchetype->GetComponent<DestroyableComponent>(1));
	LOG("Destroyable component is of %ul size!", destCompSize);
	auto nEntries = baseArchetype->Container<DestroyableComponent>().capacity();
	LOG("Archetype has %d entries!", nEntries);
}

TEST_F(ArchetypeTest, ArchetypeDeleteTest) {
	// Create a destroyable component and simple physics component and test their parent entity was assigned correctly
	const auto baseEntityId = baseEntity->GetObjectID();
	baseArchetype->CreateComponents(DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));

	// Add another entry to the archetype
	const auto newEntityId = newEntity->GetObjectID();
	baseArchetype->CreateComponents(DestroyableComponent(newEntityId), SimplePhysicsComponent(newEntityId, 2));

	// Try deleting elements
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	//Deleting more entries than exist will cause a segfault currently. Wonder if that's actually an issue...
}

TEST_F(ArchetypeTest, ReadFromArchetypeTest) {
	auto& simplePhysicsContainer = baseArchetype->Container<SimplePhysicsComponent>();
	auto& destroyableContainer = baseArchetype->Container<DestroyableComponent>();

	std::vector<std::unique_ptr<Entity>> tempEntity; // Vector of temporary entities (so they die when this test goes out of scope)

	LOG("Number of entries per vector: %d", 1000); //simplePhysicsContainer.capacity()
	srand(time(NULL));
	for (auto i = 0; i < 1000; ++i) {
		tempEntity.emplace_back(std::make_unique<Entity>(rand() + i, GameDependenciesTest::info)); // Create a new entity

		const auto tempEntityId = tempEntity[i]->GetObjectID();
		baseArchetype->CreateComponents(DestroyableComponent(tempEntityId), SimplePhysicsComponent(tempEntityId, 2));
	}

	// Benchmarking
	constexpr uint32_t nLoops = 100; //1E6;
	auto begin = std::chrono::high_resolution_clock::now();
	for (auto i = 0; i < nLoops; ++i) {
		for (const auto& destComp : destroyableContainer) {
			ASSERT_NE(destComp.GetParent(), nullptr);
		}

		for (const auto& simplePhysComp : simplePhysicsContainer) {
			ASSERT_NE(simplePhysComp.GetParent(), nullptr);
		}
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
	LOG("Total time: %lld μs", time);
	LOG("Time per loop: %lld μs", time / nLoops);
}

/*TEST_F(ArchetypeTest, AddEntityTest) {
	// Create the archetypes
	std::array<std::unique_ptr<IArchetype>, 3> tempArchetype;
	tempArchetype[0] = std::make_unique<Archetype<SimplePhysicsComponent>>(1);
	tempArchetype[1] = std::make_unique<Archetype<DestroyableComponent, SimplePhysicsComponent>>(2);
	tempArchetype[2] = std::make_unique<Archetype<DestroyableComponent>>(3);

	// Add the matching types to the component type index
	componentTypeIndex[std::type_index(typeid(SimplePhysicsComponent))] = { 1, 2 }; // TODO: USE EMPLACE
	componentTypeIndex[std::type_index(typeid(DestroyableComponent))] = { 2, 3 }; // TODO: USE EMPLACE

	// Validate
	for (auto& tempArc : tempArchetype) {
		ASSERT_NE(tempArc.get(), nullptr);
	}

	std::array<std::unique_ptr<Entity>, 3> tempEntity; // Array of temporary entities (so they die when this test goes out of scope)
	for (auto i = 0; i < 3; ++i) {
		tempEntity[i] = std::make_unique<Entity>(i, GameDependenciesTest::info);
	}

	// Get object IDs
	const LWOOBJID entityOneId = tempEntity[0]->GetObjectID();
	const LWOOBJID entityTwoId = tempEntity[1]->GetObjectID();
	const LWOOBJID entityThreeId = tempEntity[2]->GetObjectID();

	// Assert no duplicates
	ASSERT_NE(entityOneId, entityTwoId);
	ASSERT_NE(entityTwoId, entityThreeId);
	ASSERT_NE(entityOneId, entityThreeId);

	// Set component ids
	uint32_t entitySPCompOneId = 10;
	uint32_t entitySPCompTwoId = 11;

	// Create references to the unique archetype ptrs
	auto& archetypeOne = static_cast<Archetype<SimplePhysicsComponent>&>(*tempArchetype[0].get());
	auto& archetypeTwo = static_cast<Archetype<DestroyableComponent, SimplePhysicsComponent>&>(*tempArchetype[1].get());
	auto& archetypeThree = static_cast<Archetype<DestroyableComponent>&>(*tempArchetype[2].get());

	archetypeOne.CreateComponents(SimplePhysicsComponent(entityOneId, entitySPCompOneId));
	archetypeTwo.CreateComponents(DestroyableComponent(entityTwoId), SimplePhysicsComponent(entityTwoId, entitySPCompTwoId));
	archetypeThree.CreateComponents(DestroyableComponent(entityThreeId));

	// Manually create the records
	auto recordOne = ArchetypeRecord{ tempArchetype[0].get(), 0 };
	auto recordTwo = ArchetypeRecord{ tempArchetype[1].get(), 0 };
	auto recordThree = ArchetypeRecord{ tempArchetype[2].get(), 0 };

	// Manually create the corresponding pointers in the entity index
	entityIndex.insert({ entityOneId, ArchetypeRecord{ tempArchetype[0].get(), 0 }; });

	entityIndex.insert({ entityOneId, recordOne });
	entityIndex.insert({ entityTwoId, recordTwo });
	entityIndex.insert({ entityThreeId, recordThree });

	// Check that the newly-created pointers point to the same memory address as the pointers
	ASSERT_NE(entityIndex[entityOneId].archetype, nullptr);
	ASSERT_EQ(entityIndex[entityOneId].archetype, &archetypeOne);
	ASSERT_EQ(entityIndex[entityTwoId].archetype, &archetypeTwo);
	ASSERT_EQ(entityIndex[entityThreeId].archetype, &archetypeThree);

	auto* const gottenComponent = EntitySystem::GetComponent<SimplePhysicsComponent>(entityOneId);
	ASSERT_EQ(gottenComponent->GetParent()->GetObjectID(), entityOneId);
	// Find the archetype containing entity one
	// Find the indice within the archetype containing entity one

	ASSERT_FALSE(EntitySystem::HasComponent<DestroyableComponent>(entityOneId));
	ASSERT_TRUE(EntitySystem::HasComponent<SimplePhysicsComponent>(entityOneId));
}

TEST_F(ArchetypeTest, GetArchetypeTest) {
	auto& archetype = EntitySystem::GetArchetype<DestroyableComponent, SimplePhysicsComponent>();

	const auto baseEntityId = baseEntity->GetObjectID();
	archetype.CreateComponents(DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));

	auto& sameArchetype = EntitySystem::GetArchetype<DestroyableComponent, SimplePhysicsComponent>();
	auto sameEntityId = sameArchetype.GetComponent<DestroyableComponent>(0).GetParent()->GetObjectID();
	ASSERT_EQ(sameEntityId, baseEntityId);
}

TEST_F(ArchetypeTest, CreateArchetypesTest) {
	EntitySystem::CreateArchetypes<CharacterComponent, DestroyableComponent, SimplePhysicsComponent>();

	const auto baseEntityId = baseEntity->GetObjectID();
	EntitySystem::CreateComponents<DestroyableComponent, SimplePhysicsComponent>(baseEntityId, DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 12));

	const auto gottenEntityId = EntitySystem::GetArchetype<DestroyableComponent, SimplePhysicsComponent>().GetComponent<DestroyableComponent>(0).GetParent()->GetObjectID();
	ASSERT_EQ(gottenEntityId, baseEntityId);
}*/

TEST_F(ArchetypeTest, MoveIArchetypeTest) {
	// Insert an entry into the base archetype and set one trait for each
	const auto baseEntityId = baseEntity->GetObjectID();
	baseArchetype->CreateComponents(DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));
	baseArchetype->Container<DestroyableComponent>()[0].SetMaxHealth(30);
	baseArchetype->Container<SimplePhysicsComponent>()[0].SetPosition(NiPoint3(1.0f, 2.0f, 3.0f));

	// Move the archetype and compare the entry results, as well as checking the original is deleted
	std::unique_ptr<IArchetype> movedArchetype = std::move(baseArchetype);
	ASSERT_FLOAT_EQ(movedArchetype->Container<DestroyableComponent>()[0].GetMaxHealth(), 30);
	ASSERT_EQ(movedArchetype->Container<SimplePhysicsComponent>()[0].GetPosition(), NiPoint3(1.0f, 2.0f, 3.0f));
	ASSERT_EQ(baseArchetype.get(), nullptr);
}

TEST_F(ArchetypeTest, CreateEntityTest) {
	auto entitySystem = std::make_unique<EntitySystem>();

	const auto baseEntityId = baseEntity->GetObjectID();
	const auto newEntityId = newEntity->GetObjectID();

	ASSERT_NO_FATAL_FAILURE(entitySystem->CreateEntity(baseEntityId, DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2)));
}

TEST_F(ArchetypeTest, HasComponentTest) {
	auto entitySystem = std::make_unique<EntitySystem>();

	const auto baseEntityId = baseEntity->GetObjectID();
	const auto newEntityId = newEntity->GetObjectID();

	entitySystem->CreateEntity(baseEntityId, DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));

	ASSERT_TRUE(entitySystem->HasComponent<DestroyableComponent>(baseEntityId));
	ASSERT_TRUE(entitySystem->HasComponent<SimplePhysicsComponent>(baseEntityId));
	ASSERT_FALSE(entitySystem->HasComponent<CharacterComponent>(baseEntityId));
}

TEST_F(ArchetypeTest, GetComponentTest) {
	auto entitySystem = std::make_unique<EntitySystem>();

	const auto baseEntityId = baseEntity->GetObjectID();
	const auto newEntityId = newEntity->GetObjectID();

	entitySystem->CreateEntity(baseEntityId, DestroyableComponent(baseEntityId), SimplePhysicsComponent(baseEntityId, 2));

	ASSERT_NE(entitySystem->GetComponent<DestroyableComponent>(baseEntityId), nullptr);
	ASSERT_NE(entitySystem->GetComponent<SimplePhysicsComponent>(baseEntityId), nullptr);
	ASSERT_EQ(entitySystem->GetComponent<CharacterComponent>(baseEntityId), nullptr);
}
