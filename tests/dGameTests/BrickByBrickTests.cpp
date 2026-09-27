#include "BrickByBrick.h"
#include "BbbAutosaveItems.h"
#include "Database.h"
#include "Entity.h"
#include "GameDependencies.h"
#include "eInventoryType.h"

#include <gtest/gtest.h>

// The brick by brick workflow's rules (docs/BuildWorkflow.md): what happens to models and bricks at each step.
class BrickByBrickTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

namespace {
	const std::string HEADER_ONLY("sd0\x01\xff", 5);
	const std::string ONE_CHUNK("sd0\x01\xff\x02\x00\x00\x00\x78\x9c", 11);
}

TEST_F(BrickByBrickTests, EmptyModelIsNothingOrTheBareHeader) {
	EXPECT_TRUE(BrickByBrick::IsEmptyModel(""));
	EXPECT_TRUE(BrickByBrick::IsEmptyModel(HEADER_ONLY));
	EXPECT_FALSE(BrickByBrick::IsEmptyModel(ONE_CHUNK));
}

TEST_F(BrickByBrickTests, TakingAModelOffThePropertyPutsItInModels) {
	using enum BrickByBrick::eDeleteReason;
	const auto pickUp = BrickByBrick::PlanModelRemoval(static_cast<int32_t>(PICKING_MODEL_UP));
	EXPECT_TRUE(pickUp.equip);
	EXPECT_TRUE(pickUp.notifyPostDelete);

	const auto putAway = BrickByBrick::PlanModelRemoval(static_cast<int32_t>(RETURNING_MODEL_TO_INVENTORY));
	EXPECT_FALSE(putAway.equip);
	EXPECT_FALSE(putAway.notifyPostDelete);

	// Taking a model apart: into MODELS, not carried; the client then opens it with BBBLoadItemRequest
	const auto apart = BrickByBrick::PlanModelRemoval(static_cast<int32_t>(BREAKING_MODEL_APART));
	EXPECT_FALSE(apart.equip);
	EXPECT_TRUE(apart.notifyPostDelete);

	const auto unknown = BrickByBrick::PlanModelRemoval(7);
	EXPECT_FALSE(unknown.equip);
}

TEST_F(BrickByBrickTests, LeavingWithoutAnAutosaveGivesEveryModelBack) {
	const auto recovery = BrickByBrick::PlanRecovery({ 1, 2 }, std::nullopt);
	EXPECT_FALSE(recovery.rebuild);
	EXPECT_TRUE(recovery.consume.empty());
	EXPECT_EQ(recovery.giveBack, (std::vector<LWOOBJID>{ 1, 2 }));
}

TEST_F(BrickByBrickTests, AnAutosaveReplacesTheModelsItWasMadeFrom) {
	IBbbAutosave::Info autosave{ ONE_CHUNK, { 2, 3 }, 0 };
	const auto recovery = BrickByBrick::PlanRecovery({ 1, 2 }, autosave);
	EXPECT_TRUE(recovery.rebuild);
	// 3 is no longer in the BBB inventory (e.g. back in MODELS after a reload) but still made the autosave
	EXPECT_EQ(recovery.consume, (std::vector<LWOOBJID>{ 2, 3 }));
	// 1 was opened after the autosave: it goes back
	EXPECT_EQ(recovery.giveBack, (std::vector<LWOOBJID>{ 1 }));
}

TEST_F(BrickByBrickTests, AnEmptyAutosaveRebuildsNothing) {
	IBbbAutosave::Info autosave{ HEADER_ONLY, { 1 }, 0 };
	const auto recovery = BrickByBrick::PlanRecovery({ 1 }, autosave);
	EXPECT_FALSE(recovery.rebuild);
	EXPECT_TRUE(recovery.consume.empty());
	EXPECT_EQ(recovery.giveBack, (std::vector<LWOOBJID>{ 1 }));
}

TEST_F(BrickByBrickTests, BuildInventoriesLoadIntoTheNormalOnes) {
	EXPECT_EQ(BrickByBrick::InventoryToLoadInto(eInventoryType::MODELS_IN_BBB), eInventoryType::MODELS);
	EXPECT_EQ(BrickByBrick::InventoryToLoadInto(eInventoryType::BRICKS_IN_BBB), eInventoryType::BRICKS);
	for (const auto type : { eInventoryType::ITEMS, eInventoryType::BRICKS, eInventoryType::MODELS, eInventoryType::VAULT_MODELS, eInventoryType::TEMP_MODELS }) {
		EXPECT_EQ(BrickByBrick::InventoryToLoadInto(type), type);
	}
}

TEST_F(BrickByBrickTests, ModelItemConfigKeepsTheModelsIds) {
	const auto config = BrickByBrick::ModelItemConfig(1152921508340227662, 1152921510759098851, "10447,0,0,0,0");
	EXPECT_EQ(BrickByBrick::ConfigObjectId(config, u"blueprintid"), 1152921508340227662);
	EXPECT_EQ(BrickByBrick::ConfigObjectId(config, u"userModelID"), 1152921510759098851);
	EXPECT_EQ(config.find(u"userModelBehaviors")->second->GetValueAsString(), "10447,0,0,0,0");
	EXPECT_EQ(config.find(u"userModelHasBhvr")->second->GetValueAsString(), "1");
	EXPECT_EQ(config.find(u"userModelPhysicsType")->second->GetValueAsString(), "2");

	const auto plain = BrickByBrick::ModelItemConfig(5, 6);
	EXPECT_EQ(plain.find(u"userModelHasBhvr")->second->GetValueAsString(), "0");
	EXPECT_EQ(plain.find(u"userModelBehaviors")->second->GetValueAsString(), "0,0,0,0,0");
	EXPECT_EQ(BrickByBrick::ConfigObjectId(plain, u"missing"), LWOOBJID_EMPTY);
}

TEST_F(BrickByBrickTests, AutosaveItemsListRoundTrips) {
	const std::vector<LWOOBJID> items = { 1152921510759098815, 2, 3 };
	EXPECT_EQ(BbbAutosaveItems::Join(items), "1152921510759098815,2,3");
	EXPECT_EQ(BbbAutosaveItems::Parse(BbbAutosaveItems::Join(items)), items);
	EXPECT_TRUE(BbbAutosaveItems::Parse("").empty());
	EXPECT_EQ(BbbAutosaveItems::Parse("4,,x,5"), (std::vector<LWOOBJID>{ 4, 5 }));
}

TEST_F(BrickByBrickTests, QuickSaveIsKeptUntilClearedWithTheBareHeader) {
	Entity player(1152921510000000001, info);
	BrickByBrick::Autosave(player, ONE_CHUNK);
	const auto saved = Database::Get()->GetBbbAutosave(player.GetObjectID());
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->lxfml, ONE_CHUNK);
	EXPECT_GT(saved->updatedAt, 0);

	BrickByBrick::Autosave(player, HEADER_ONLY);
	EXPECT_FALSE(Database::Get()->GetBbbAutosave(player.GetObjectID()).has_value());
}

TEST_F(BrickByBrickTests, AReturnedModelIsNeverUsedUpByTheAutosave) {
	Entity player(1152921510000000002, info);
	Database::Get()->SetBbbAutosave(player.GetObjectID(), { ONE_CHUNK, { 7, 8 }, 1 });
	BrickByBrick::ReturnModel(player, 7, false, NiPoint3Constant::ZERO, QuatUtils::IDENTITY);
	const auto saved = Database::Get()->GetBbbAutosave(player.GetObjectID());
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->sourceItems, (std::vector<LWOOBJID>{ 8 }));
}
