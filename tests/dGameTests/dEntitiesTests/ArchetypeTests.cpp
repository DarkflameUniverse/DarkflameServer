#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Logger.h"
#include "Game.h"
#include "BitStream.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"
#include "Archetype.h"

class ArchetypeTest : public GameDependenciesTest {
protected:
	Entity* baseEntity;
	Entity* newEntity;

	std::unique_ptr<Archetype<DestroyableComponent, SimplePhysicsComponent>> baseArchetype;

	std::unique_ptr<Logger> logger = std::make_unique<Logger>("./ArchetypeTests", true, true);

	CBITSTREAM

		void SetUp() override {
		SetUpDependencies();

		Game::logger = logger.get();

		baseEntity = new Entity(15, GameDependenciesTest::info);
		newEntity = new Entity(16, GameDependenciesTest::info);

		baseArchetype = std::make_unique<Archetype<DestroyableComponent, SimplePhysicsComponent>>(0);
	}

	void TearDown() override {
		Game::logger = nullptr;

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
	const NiPoint3 testPos{ -1.0, 1.0, 3.0 };
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

	size_t destCompSize = sizeof(baseArchetype->GetComponent<DestroyableComponent>(1));
	LOG("Destroyable component is of %ul size!", destCompSize);
	auto nEntries = baseArchetype->ComponentContainer<DestroyableComponent>().capacity();
	LOG("Archetype has %d entries!", nEntries);

	// Try deleting elements
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	ASSERT_NO_THROW(baseArchetype->DeleteComponents(0));
	ASSERT_ANY_THROW(baseArchetype->DeleteComponents(0));
}

TEST_F(ArchetypeTest, ReadFromArchetypeTest) {
	auto& simplePhysicsContainer = baseArchetype->ComponentContainer<SimplePhysicsComponent>();
	auto& destroyableContainer = baseArchetype->ComponentContainer<DestroyableComponent>();

	std::vector<std::unique_ptr<Entity>> tempEntity; // Vector of temporary entities (so they die when this test goes out of scope)

	constexpr uint32_t nLoops = 1E6;

	for (auto i = 0; i < simplePhysicsContainer.capacity(); ++i) {
		tempEntity.emplace_back(std::make_unique<Entity>(i, GameDependenciesTest::info)); // Create a new entity

		Entity* tempEntityPtr = tempEntity[i].get();
		baseArchetype->CreateComponents(DestroyableComponent(tempEntityPtr), SimplePhysicsComponent(tempEntityPtr, 2));
	}

	// Benchmarking
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

	constexpr NiPoint3 test{1.0f, 1.0f, 1.0f};
	ASSERT_EQ(test.x, 1.0f);
}
