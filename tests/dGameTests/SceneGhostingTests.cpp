#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Entity.h"
#include "ZoneScenes.h"

class SceneGhostingTest : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// Objects keep the scene they were placed in, which scene ghosting goes by; objects from no scene go by their position
TEST_F(SceneGhostingTest, EntitiesKeepTheirScene) {
	EntityInfo placed = info;
	placed.scene = 12;
	const auto fromScene = std::make_unique<Entity>(20, placed);
	EXPECT_EQ(fromScene->GetScene(), 12);
	const auto spawned = std::make_unique<Entity>(21, info);
	EXPECT_EQ(spawned->GetScene(), -1);

	// A player standing in scene 3, whose client keeps 0, 3 and 5 loaded
	const std::set<uint32_t> loaded{ 0, 3, 5 };
	EXPECT_FALSE(ZoneScenes::InLoadedScene(fromScene->GetScene(), 3, loaded)); // placed in scene 12, though it stands in 3
	EXPECT_TRUE(ZoneScenes::InLoadedScene(spawned->GetScene(), 5, loaded));
	EXPECT_FALSE(ZoneScenes::InLoadedScene(spawned->GetScene(), 12, loaded));
}
