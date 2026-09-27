#include <gtest/gtest.h>

#include "dpCollisionFilter.h"
#include "dpEntity.h"
#include "dpShapeBox.h"

using namespace dpCollisionFilter;

namespace {
	constexpr uint32_t PLAYER = 10;
	constexpr uint32_t ENEMY = 12;
}

TEST(CollisionFilterTests, GroupZeroTouchesEverything) {
	EXPECT_TRUE(ShouldCollide(0, PLAYER));
	EXPECT_TRUE(ShouldCollide(ENEMY, 0));
	EXPECT_TRUE(ShouldCollide(0, 0));
}

TEST(CollisionFilterTests, TableIsSymmetric) {
	for (uint32_t a = 1; a <= GROUP_COUNT; a++) {
		for (uint32_t b = 1; b <= GROUP_COUNT; b++) EXPECT_EQ(GroupsCollide(a, b), GroupsCollide(b, a)) << a << " " << b;
	}
}

TEST(CollisionFilterTests, LiveProximityGroups) {
	// The AM shield generator: group 10 to find enemies, group 1 to find players
	EXPECT_TRUE(ShouldCollide(10, ENEMY));
	EXPECT_FALSE(ShouldCollide(10, PLAYER));
	EXPECT_TRUE(ShouldCollide(1, PLAYER));
	EXPECT_FALSE(ShouldCollide(1, ENEMY));
}

TEST(CollisionFilterTests, TriggerWalls) {
	// POI trigger walls (group 1) ignore enemies; "Clear threat list" walls (18) only catch enemies
	EXPECT_FALSE(ShouldCollide(1, ENEMY));
	EXPECT_TRUE(ShouldCollide(18, ENEMY));
	EXPECT_FALSE(ShouldCollide(18, PLAYER));
	// Death volumes (4) catch both
	EXPECT_TRUE(ShouldCollide(4, PLAYER));
	EXPECT_TRUE(ShouldCollide(4, ENEMY));
	// Groups 21 and 25 touch nothing
	EXPECT_FALSE(ShouldCollide(21, PLAYER));
	EXPECT_FALSE(ShouldCollide(25, ENEMY));
}

TEST(CollisionFilterTests, Flags) {
	// Two phantom-only objects never touch, one is ignored
	EXPECT_FALSE(ShouldCollide(PHANTOM_ONLY | 4, PHANTOM_ONLY | PLAYER));
	EXPECT_TRUE(ShouldCollide(PHANTOM_ONLY | 4, PLAYER));
	// A group mask touches exactly the groups in it
	const auto playersOnly = GROUP_MASK | (1u << (PLAYER - 1));
	EXPECT_TRUE(ShouldCollide(playersOnly, PLAYER));
	EXPECT_FALSE(ShouldCollide(ENEMY, playersOnly));
	EXPECT_FALSE(ShouldCollide(playersOnly, playersOnly));
	// Parts of the same system don't touch each other
	EXPECT_FALSE(ShouldCollide(0x08000000 | ENEMY, 0x08000000 | ENEMY));
	EXPECT_TRUE(ShouldCollide(0x08000000 | ENEMY, 0x10000000 | ENEMY));
}

TEST(CollisionFilterTests, RotatedBoxesKeepTheirShape) {
	// A 40 long, 1 thick wall turned 45 degrees: its axis aligned bounds are a ~29x29 square, but only the wall counts
	dpEntity wall(1, 40.0f, 10.0f, 1.0f);
	wall.SetRotation(QuatUtils::AxisAngle(NiPoint3(0.0f, 1.0f, 0.0f), glm::radians(45.0f)));
	wall.SetPosition(NiPoint3(100.0f, 0.0f, 100.0f));
	auto* box = static_cast<dpShapeBox*>(wall.GetShape());

	// Along the wall
	const auto along = QuatUtils::AxisAngle(NiPoint3(0.0f, 1.0f, 0.0f), glm::radians(45.0f));
	const auto onWall = NiPoint3(100.0f, 5.0f, 100.0f) + NiPoint3(15.0f, 0.0f, 0.0f).RotateByQuaternion(along);
	EXPECT_FLOAT_EQ(box->SquaredDistanceTo(onWall), 0.0f);

	// A corner of the old axis aligned box, far from the wall
	EXPECT_GT(box->SquaredDistanceTo(NiPoint3(112.0f, 5.0f, 112.0f)), 100.0f);

	// Above the top and below the bottom
	EXPECT_NEAR(box->SquaredDistanceTo(NiPoint3(100.0f, 12.0f, 100.0f)), 4.0f, 1e-3f);
	EXPECT_NEAR(box->SquaredDistanceTo(NiPoint3(100.0f, -3.0f, 100.0f)), 9.0f, 1e-3f);
}

#include "dpWorld.h"
#include "dpGrid.h"

TEST(CollisionFilterTests, DetachedVolumesStopColliding) {
	dpGrid grid(8, 100);
	auto* volume = new dpEntity(1, 5.0f);
	auto* player = new dpEntity(2, 1.0f, false);
	volume->SetPosition(NiPoint3Constant::ZERO);
	player->SetPosition(NiPoint3(1.0f, 0.0f, 0.0f));
	volume->SetGrid(&grid);
	player->SetGrid(&grid);

	grid.Update(0.1f);
	ASSERT_EQ(volume->GetNewObjects().size(), 1u);

	// Switched off: out of the grid, moving it doesn't sneak it back in, and it stops seeing anything
	grid.Remove(volume);
	volume->ClearCollisions();
	volume->SetPosition(NiPoint3(0.5f, 0.0f, 0.0f));
	grid.Update(0.1f);
	EXPECT_TRUE(volume->GetCurrentlyCollidingObjects().empty());

	// Switched back on: sees the player again
	volume->SetGrid(&grid);
	grid.Update(0.1f);
	EXPECT_EQ(volume->GetNewObjects().size(), 1u);

	grid.Delete(volume);
	grid.Delete(player);
}
