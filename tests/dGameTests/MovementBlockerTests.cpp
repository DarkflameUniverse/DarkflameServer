#include <gtest/gtest.h>

#include <memory>

#include "dpEntity.h"
#include "dpMovementBlockers.h"
#include "dpShapeBox.h"
#include "NiQuaternion.h"

using namespace dpMovementBlockers;

namespace {
	constexpr uint32_t PLAYER = 10;
	constexpr uint32_t ENEMY = 12;

	// A wall placed the way the physics components place one: scaled, turned, then moved
	std::unique_ptr<dpEntity> Wall(const NiPoint3& position, const NiQuaternion& rotation, float scale, float width = 10.0f, float height = 5.0f, float depth = 1.0f) {
		auto wall = std::make_unique<dpEntity>(1, width, height, depth);
		wall->SetScale(scale);
		wall->SetRotation(rotation);
		wall->SetPosition(position);
		return wall;
	}

	// Turned 90 degrees about Y, as the Sentinel camp walls: its width runs along world z
	NiQuaternion Quarter() { return NiQuaternion(0.7071068f, 0.0f, -0.7071068f, 0.0f); }
}

TEST(MovementBlockerTests, BlockingFilterFromData) {
	// Navmesh carvers stop every mover, whatever their group
	EXPECT_EQ(BlockingFilter(true, false, 7), 0u);
	EXPECT_EQ(BlockingFilter(true, true, 1), 0u);
	// Solid group 18 volumes (FV - Enemy Blocking Volume, PR - Pet Blocker) touch enemies and not players
	EXPECT_EQ(BlockingFilter(false, true, 18), 18u);
	// A phantom group 18 volume ("Clear threat list Trigger Wall") is a trigger, not a wall
	EXPECT_FALSE(BlockingFilter(false, false, 18).has_value());
	// Ordinary solid objects players collide with are left to the navmesh
	EXPECT_FALSE(BlockingFilter(false, true, 1).has_value());
	EXPECT_FALSE(BlockingFilter(false, true, 0).has_value());
}

TEST(MovementBlockerTests, SegmentEntryOnTurnedWall) {
	// 10 x 5 x 1 scaled 2.5 at x = 134: 25 long along z, 2.5 thick along x, 12.5 tall
	const auto wall = Wall({ 134.0f, 375.0f, -200.0f }, Quarter(), 2.5f);
	const auto* box = dynamic_cast<const dpShapeBox*>(wall->GetShape());
	ASSERT_NE(box, nullptr);

	// Straight across it, from x = 124 to 144: enters at its near face, x = 132.75
	const auto hit = box->SegmentEntry({ 124.0f, 376.0f, -200.0f }, { 144.0f, 376.0f, -200.0f });
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(*hit, (132.75f - 124.0f) / 20.0f, 1e-3f);

	// Past its end (z = -215 is beyond -200 +- 12.5)
	EXPECT_FALSE(box->SegmentEntry({ 124.0f, 376.0f, -215.0f }, { 144.0f, 376.0f, -215.0f }).has_value());
	// Along it without touching
	EXPECT_FALSE(box->SegmentEntry({ 130.0f, 376.0f, -210.0f }, { 130.0f, 376.0f, -190.0f }).has_value());
	// Over it
	EXPECT_FALSE(box->SegmentEntry({ 124.0f, 390.0f, -200.0f }, { 144.0f, 390.0f, -200.0f }).has_value());
	// Starting inside it: free to walk out
	EXPECT_FALSE(box->SegmentEntry({ 134.0f, 376.0f, -200.0f }, { 144.0f, 376.0f, -200.0f }).has_value());
}

TEST(MovementBlockerTests, FilterDecidesWhoIsBlocked) {
	const auto wall = Wall({ 0.0f, 0.0f, 0.0f }, QuatUtils::IDENTITY, 1.0f);
	const std::vector<dpMovementBlocker> enemyOnly{ { wall.get(), 18 } };
	const NiPoint3 a{ 0.0f, 0.0f, -5.0f };
	const NiPoint3 b{ 0.0f, 0.0f, 5.0f };

	EXPECT_TRUE(FirstHit(enemyOnly, a, b, ENEMY).has_value());
	EXPECT_FALSE(FirstHit(enemyOnly, a, b, PLAYER).has_value());

	const std::vector<dpMovementBlocker> carver{ { wall.get(), 0 } };
	EXPECT_TRUE(FirstHit(carver, a, b, ENEMY).has_value());
	EXPECT_TRUE(FirstHit(carver, a, b, PLAYER).has_value());
}

TEST(MovementBlockerTests, ClampPathStopsBeforeTheWall) {
	// A wall across z = 0 (x from -5 to 5, 1 thick), and an enemy walking from z = -10 to z = 10 in two legs
	const auto wall = Wall({ 0.0f, 0.0f, 0.0f }, QuatUtils::IDENTITY, 1.0f);
	const std::vector<dpMovementBlocker> blockers{ { wall.get(), 0 } };
	const NiPoint3 start{ 0.0f, 0.0f, -10.0f };

	const auto path = ClampPath(blockers, start, { { 0.0f, 0.0f, -8.0f }, { 0.0f, 0.0f, 10.0f } }, ENEMY);
	ASSERT_EQ(path.size(), 2u);
	EXPECT_FLOAT_EQ(path[0].z, -8.0f);
	// The wall's near face is z = -0.5; the path ends STOP_DISTANCE before it
	EXPECT_NEAR(path[1].z, -0.5f - STOP_DISTANCE, 1e-3f);

	// Around the end of the wall is fine
	const auto around = ClampPath(blockers, start, { { 8.0f, 0.0f, -2.0f }, { 8.0f, 0.0f, 2.0f }, { 0.0f, 0.0f, 10.0f } }, ENEMY);
	EXPECT_EQ(around.size(), 3u);

	// Right up against the wall: nothing left to walk
	const auto against = ClampPath(blockers, { 0.0f, 0.0f, -1.2f }, { { 0.0f, 0.0f, 10.0f } }, ENEMY);
	EXPECT_TRUE(against.empty());

	// No blockers, no change
	const auto free = ClampPath({}, start, { { 0.0f, 0.0f, 10.0f } }, ENEMY);
	EXPECT_EQ(free.size(), 1u);
}

TEST(MovementBlockerTests, ClearThreatWallIsTheRealSize) {
	// test\POI_trigger_wall.hkx: 1 x 12.98 x 20.45, placed at 2.44 on the Block Yard property, turned a quarter
	const auto wall = Wall({ -21.6f, 456.7f, -42.3f }, NiQuaternion(0.706f, 0.0f, -0.708f, 0.0f), 2.44f, 1.0f, 12.9755f, 20.45f);
	const auto* box = dynamic_cast<const dpShapeBox*>(wall->GetShape());
	ASSERT_NE(box, nullptr);
	// About 50 long along x, so an enemy walking along z 20 from its middle still walks into it
	EXPECT_NEAR(box->m_MaxX - box->m_MinX, 20.45f * 2.44f, 0.5f);
	EXPECT_TRUE(box->SegmentEntry({ -1.6f, 458.0f, -52.3f }, { -1.6f, 458.0f, -32.3f }).has_value());
}
