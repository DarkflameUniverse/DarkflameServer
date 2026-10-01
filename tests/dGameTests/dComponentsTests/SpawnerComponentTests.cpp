#include "GameDependencies.h"

#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "Entity.h"
#include "Spawner.h"
#include "SpawnerComponent.h"
#include "dZoneManager.h"

#include <gtest/gtest.h>

// A spawner object's SpawnerComponent owns its spawner; the zone manager only indexes it.
class SpawnerComponentTests : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		// The entity's own LOT has no components, so it doesn't look anything up in a database
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>().insert_or_assign(static_cast<uint64_t>(info.lot), 0);
	}

	void TearDown() override { TearDownDependencies(); }

	static SpawnerInfo InactiveSpawner(const LWOOBJID id) {
		SpawnerInfo spawnerInfo{};
		spawnerInfo.spawnerID = id;
		spawnerInfo.activeOnLoad = false;
		return spawnerInfo;
	}
};

TEST_F(SpawnerComponentTests, TheZoneManagerFindsTheComponentsSpawner) {
	constexpr LWOOBJID id = 1234;
	auto entity = std::make_unique<Entity>(id, info);
	auto* const spawnerComponent = entity->AddComponent<SpawnerComponent>(-1);
	auto spawner = std::make_unique<Spawner>(InactiveSpawner(id));
	auto* const spawnerPtr = spawner.get();
	spawnerComponent->SetSpawner(std::move(spawner));
	Game::zoneManager->AddSpawner(id, spawnerPtr);

	EXPECT_EQ(spawnerComponent->GetSpawner(), spawnerPtr);
	EXPECT_EQ(Game::zoneManager->GetSpawner(id), spawnerPtr);
}

TEST_F(SpawnerComponentTests, TheSpawnerGoesWithItsObject) {
	constexpr LWOOBJID id = 1235;
	auto entity = std::make_unique<Entity>(id, info);
	auto* const spawnerComponent = entity->AddComponent<SpawnerComponent>(-1);
	auto spawner = std::make_unique<Spawner>(InactiveSpawner(id));
	Game::zoneManager->AddSpawner(id, spawner.get());
	spawnerComponent->SetSpawner(std::move(spawner));

	entity.reset();

	EXPECT_EQ(Game::zoneManager->GetSpawner(id), nullptr);
}
