#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Logger.h"
#include "Game.h"
#include "BitStream.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"
#include "Archetype.h"
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

		const auto tempEntityId = tempEntity[i]->GetObjectID();
		baseArchetype->CreateComponents(DestroyableComponent(tempEntityId), SimplePhysicsComponent(tempEntityId, 2));
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
