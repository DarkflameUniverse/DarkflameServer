// Edit mode (docs/EditMode.md): who may enter, and a level file's objects not being made.
#include "GameDependencies.h"

#include "EditMode.h"
#include "EntityManager.h"
#include "Level.h"
#include "LevelFile.h"
#include "Zone.h"
#include "GeneralUtils.h"
#include "dConfig.h"
#include "eObjectBits.h"
#include "dZoneManager.h"

#include <gtest/gtest.h>

class EditModeTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(EditModeTests, OnlyEditorAccountsMayEnter) {
	EXPECT_TRUE(EditMode::MayEnter(eGameMasterLevel::OPERATOR, eGameMasterLevel::OPERATOR));
	EXPECT_FALSE(EditMode::MayEnter(eGameMasterLevel::DEVELOPER, eGameMasterLevel::OPERATOR));
	EXPECT_FALSE(EditMode::MayEnter(eGameMasterLevel::CIVILIAN, eGameMasterLevel::OPERATOR));
	EXPECT_FALSE(EditMode::MayEnter(eGameMasterLevel::CIVILIAN, eGameMasterLevel::CIVILIAN));
	EXPECT_TRUE(EditMode::MayEnter(eGameMasterLevel::DEVELOPER, eGameMasterLevel::JUNIOR_DEVELOPER));

	// editor_gm_level, 9 unless set
	EXPECT_EQ(EditMode::RequiredLevel(), eGameMasterLevel::OPERATOR);
	Game::config->SetDatabaseValues({ { "editor_gm_level", "8" } }, {});
	EXPECT_EQ(EditMode::RequiredLevel(), eGameMasterLevel::DEVELOPER);
	Game::config->SetDatabaseValues({ { "editor_gm_level", "0" } }, {});
	EXPECT_EQ(EditMode::RequiredLevel(), eGameMasterLevel::FORUM_MODERATOR);
	Game::config->SetDatabaseValues({}, {});
}

// In edit mode a level file's objects and spawners are not made; the player start marker still gives the spawn point
TEST_F(EditModeTests, LevelObjectsAreNotMade) {
	Game::zoneManager->SetEditMode(true);
	ASSERT_TRUE(Game::zoneManager->GetEditMode());

	std::vector<SceneObject> objects;
	SceneObject object;
	object.id = 1001;
	object.lot = 2365;
	objects.push_back(object);
	SceneObject spawner;
	spawner.id = 1002;
	spawner.lot = 176;
	objects.push_back(spawner);
	SceneObject start;
	start.id = 1003;
	start.lot = LOT_MARKER_PLAYER_START;
	start.position = NiPoint3(10.0f, 20.0f, 30.0f);
	objects.push_back(start);

	Level::LoadSceneObjects(objects, 4);

	// (a level object's ID gets the client bit when it is made)
	for (const LWOOBJID id : { 1001, 1002, 1003 }) {
		EXPECT_EQ(Game::entityManager->GetEntity(id), nullptr);
		LWOOBJID made = id;
		GeneralUtils::SetBit(made, eObjectBits::CLIENT);
		EXPECT_EQ(Game::entityManager->GetEntity(made), nullptr);
	}
	EXPECT_TRUE(Game::zoneManager->GetSpawners().empty());
	EXPECT_EQ(Game::zoneManager->GetZone()->GetSpawnPos(), NiPoint3(10.0f, 20.0f, 30.0f));
}
