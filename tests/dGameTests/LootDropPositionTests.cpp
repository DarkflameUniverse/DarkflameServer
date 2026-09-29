#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "InventoryMessages.h"
#include "Loot.h"

#include <cmath>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// DropClientLoot's use_position and final_position, as live sent them (12,501 live DropClientLoot).
class LootDropPositionTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

// A live coin drop from an enemy with the player, loot and source IDs replaced: use_position false, final
// position equal to the spawn position.
TEST_F(LootDropPositionTests, CoinsLandWhereTheySpawn) {
	GameMessages::DropClientLoot lootMsg;
	lootMsg.target = 0x1000000000000001LL;
	lootMsg.ownerID = 0x1000000000000001LL;
	lootMsg.lootID = 0x0000000011223344LL;
	lootMsg.sourceID = 0x0102030405060708LL;
	lootMsg.currency = 1;
	lootMsg.item = LOT_NULL;
	lootMsg.spawnPos = NiPoint3(46.63258361816406f, 388.45635986328125f, -108.97083282470703f);
	Loot::SetDropPositions(lootMsg, false, 1.0f);

	EXPECT_FALSE(lootMsg.bUsePosition);
	EXPECT_EQ(lootMsg.finalPosition, lootMsg.spawnPos);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 1e 00 71 21 ce 90 9a 8e b0 90 c4 7c 76 70 80 40 00 00 3f ff ff ff "
		"d1 0c c8 84 40 00 00 00 00 40 00 00 00 00 00 04 02 01 c1 81 41 00 c0 80 78 90 e7 48 4d 47 58 48 62 3e 3b 38 40", 595), StructPacket(lootMsg));
}

TEST_F(LootDropPositionTests, ItemsLandTenUnitsAwayOnTheGround) {
	for (const float angle : { 0.0f, 1.0f, 3.14159265f, 5.5f }) {
		GameMessages::DropClientLoot lootMsg;
		lootMsg.item = 935;
		lootMsg.spawnPos = NiPoint3(100.0f, 20.0f, -50.0f);
		Loot::SetDropPositions(lootMsg, false, angle);

		EXPECT_FALSE(lootMsg.bUsePosition);
		EXPECT_EQ(lootMsg.finalPosition.y, 20.0f);
		const auto dx = lootMsg.finalPosition.x - 100.0f;
		const auto dz = lootMsg.finalPosition.z + 50.0f;
		EXPECT_NEAR(std::sqrt(dx * dx + dz * dz), Loot::ITEM_DROP_DISTANCE, 1e-3f);
		EXPECT_NEAR(dx, std::sin(angle) * 10.0f, 1e-3f);
		EXPECT_NEAR(dz, std::cos(angle) * 10.0f, 1e-3f);
	}
}

TEST_F(LootDropPositionTests, OnlyAPlayerSourceUsesThePosition) {
	GameMessages::DropClientLoot lootMsg;
	lootMsg.item = 935;
	lootMsg.spawnPos = NiPoint3(1.0f, 2.0f, 3.0f);
	Loot::SetDropPositions(lootMsg, true, 0.0f);
	EXPECT_TRUE(lootMsg.bUsePosition);

	Loot::SetDropPositions(lootMsg, false, 0.0f);
	EXPECT_FALSE(lootMsg.bUsePosition);
}

TEST_F(LootDropPositionTests, NoSpawnPositionLeavesTheMessageAlone) {
	GameMessages::DropClientLoot lootMsg;
	lootMsg.item = 935;
	Loot::SetDropPositions(lootMsg, true, 1.0f);
	EXPECT_FALSE(lootMsg.bUsePosition);
	EXPECT_EQ(lootMsg.finalPosition, NiPoint3Constant::ZERO);
}
