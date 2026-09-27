#include <gtest/gtest.h>

#include <cmath>

#include "dpKnockback.h"

namespace {
	constexpr float GRAVITY = 57.0f; // WorldConfig.pegravityvalue
	constexpr float TICK = 1.0f / 30.0f;

	const dpKnockback::GroundQuery flatGround = [](const NiPoint3&) { return 0.0f; };

	// Flies the arc to the end, returns how long it took
	float Fly(dpKnockback::Arc& arc, const dpKnockback::GroundQuery& ground, float tick = TICK) {
		float time = 0.0f;
		while (arc.Step(tick, GRAVITY, ground)) {
			time += tick;
			if (time > 20.0f) break;
		}
		return time + tick;
	}
}

TEST(KnockbackTests, VectorMatchesTheClient) {
	// Flat direction away from the source, raised by angle degrees
	const auto vector = dpKnockback::ComputeVector(NiPoint3(10.0f, 3.0f, 0.0f), 45.0f, 20.0f);
	EXPECT_NEAR(vector.x, 20.0f / std::sqrt(2.0f), 1e-3f);
	EXPECT_NEAR(vector.y, 20.0f / std::sqrt(2.0f), 1e-3f);
	EXPECT_NEAR(vector.z, 0.0f, 1e-5f);

	// The dragon's slam: angle 65, strength 55
	const auto slam = dpKnockback::ComputeVector(NiPoint3(0.0f, 0.0f, -4.0f), 65.0f, 55.0f);
	EXPECT_NEAR(slam.Length(), 55.0f, 1e-3f);
	EXPECT_NEAR(std::atan2(slam.y, -slam.z) * 180.0f / 3.14159265f, 65.0f, 1e-2f);
}

TEST(KnockbackTests, NinetyDegreesIsStraightUp) {
	const auto up = dpKnockback::ComputeVector(NiPoint3(5.0f, 0.0f, 5.0f), 90.0f, 40.0f);
	EXPECT_EQ(up, NiPoint3(0.0f, 40.0f, 0.0f));

	// Without a direction the client still knocks straight up for anything near 90
	EXPECT_EQ(dpKnockback::ComputeVector(NiPoint3Constant::ZERO, 89.5f, 10.0f), NiPoint3(0.0f, 10.0f, 0.0f));
}

TEST(KnockbackTests, StrengthIsCapped) {
	const auto vector = dpKnockback::ComputeVector(NiPoint3(1.0f, 0.0f, 0.0f), 0.0f, 1000.0f);
	EXPECT_NEAR(vector.x, dpKnockback::MAX_STRENGTH, 1e-3f);
	EXPECT_NEAR(vector.y, 0.0f, 1e-5f);
}

TEST(KnockbackTests, ShortVectorsDontLaunch) {
	dpKnockback::Arc arc;
	EXPECT_FALSE(arc.Start(NiPoint3(1.0f, 2.0f, 3.0f), NiPoint3(3.0f, 0.0f, 4.0f))); // length 5
	EXPECT_FALSE(arc.IsActive());

	// Moved by the vector and dropped onto the ground when it's close
	const auto moved = dpKnockback::Nudge(NiPoint3(0.0f, 0.5f, 0.0f), NiPoint3(3.0f, 0.0f, 0.0f), 2.0f, flatGround);
	EXPECT_EQ(moved, NiPoint3(3.0f, 0.0f, 0.0f));

	// Ground too far below (a ledge): stays put, like the client
	const auto ledge = dpKnockback::Nudge(NiPoint3(0.0f, 10.0f, 0.0f), NiPoint3(3.0f, 0.0f, 0.0f), 2.0f, flatGround);
	EXPECT_EQ(ledge, NiPoint3(0.0f, 10.0f, 0.0f));
}

TEST(KnockbackTests, ArcFollowsBallistics) {
	// Straight up at 57 u/s under 57 u/s^2 rises for 1s, peaks at 28.5 above the 0.5 lift, lands after ~2s
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(0.0f, 57.0f, 0.0f)));
	EXPECT_FLOAT_EQ(arc.GetPosition().y, dpKnockback::LAUNCH_LIFT);

	float peak = 0.0f;
	float time = 0.0f;
	while (arc.Step(TICK, GRAVITY, flatGround)) {
		time += TICK;
		peak = std::max(peak, arc.GetPosition().y);
	}
	EXPECT_NEAR(peak, 28.5f + dpKnockback::LAUNCH_LIFT, 1.0f);
	EXPECT_NEAR(time, 2.0f, 0.1f);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.0f);
	EXPECT_EQ(arc.GetVelocity(), NiPoint3Constant::ZERO);
}

TEST(KnockbackTests, HorizontalDistanceMatchesFlightTime) {
	// The shield generator: 50 back, 15 up
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(-50.0f, 15.0f, 0.0f)));
	const auto time = Fly(arc, flatGround);

	// t = (vy + sqrt(vy^2 + 2 g lift)) / g
	const float expected = (15.0f + std::sqrt(15.0f * 15.0f + 2.0f * GRAVITY * dpKnockback::LAUNCH_LIFT)) / GRAVITY;
	EXPECT_NEAR(time, expected, 2 * TICK);
	EXPECT_NEAR(arc.GetPosition().x, -50.0f * expected, 50.0f * 2 * TICK);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.0f);
}

TEST(KnockbackTests, TickRateDoesntChangeTheLanding) {
	dpKnockback::Arc slow;
	dpKnockback::Arc fast;
	ASSERT_TRUE(slow.Start(NiPoint3Constant::ZERO, NiPoint3(20.0f, 30.0f, 10.0f)));
	ASSERT_TRUE(fast.Start(NiPoint3Constant::ZERO, NiPoint3(20.0f, 30.0f, 10.0f)));
	Fly(slow, flatGround, 0.1f);
	Fly(fast, flatGround, 1.0f / 120.0f);
	EXPECT_NEAR(slow.GetPosition().x, fast.GetPosition().x, 0.5f);
	EXPECT_NEAR(slow.GetPosition().z, fast.GetPosition().z, 0.5f);
}

TEST(KnockbackTests, FlatKnockbackLastsTheMinimumTime) {
	// Mostly sideways: lands from the 0.5 lift almost at once but the client keeps the state 250ms
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(0.0f, 0.0f, 40.0f)));
	while (arc.Step(1.0f / 100.0f, GRAVITY, flatGround)) {}
	EXPECT_GE(arc.GetElapsed(), dpKnockback::MIN_AIRBORNE_TIME);
	EXPECT_LT(arc.GetElapsed(), dpKnockback::MIN_AIRBORNE_TIME + 0.02f);
	EXPECT_NEAR(arc.GetPosition().z, 40.0f * arc.GetElapsed(), 0.5f);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.0f);
}

TEST(KnockbackTests, WallsStopSidewaysMotion) {
	// A 10 tall wall at x = 5
	const dpKnockback::GroundQuery wall = [](const NiPoint3& point) { return point.x >= 5.0f ? 10.0f : 0.0f; };
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(40.0f, 10.0f, 0.0f)));
	Fly(arc, wall);
	EXPECT_LT(arc.GetPosition().x, 5.0f);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.0f);
}

TEST(KnockbackTests, LandsOnLowerGround) {
	// Knocked off a 5 high ledge at x = 2
	const dpKnockback::GroundQuery ledge = [](const NiPoint3& point) { return point.x < 2.0f ? 5.0f : 0.0f; };
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3(0.0f, 5.0f, 0.0f), NiPoint3(10.0f, 5.0f, 0.0f)));
	Fly(arc, ledge);
	EXPECT_GT(arc.GetPosition().x, 2.0f);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.0f);
}

TEST(KnockbackTests, StepsUpOntoLowRises) {
	// A 0.5 step at x = 1 is walked onto, not treated as a wall
	const dpKnockback::GroundQuery step = [](const NiPoint3& point) { return point.x >= 1.0f ? 0.5f : 0.0f; };
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(20.0f, 0.0f, 0.0f)));
	Fly(arc, step);
	EXPECT_GT(arc.GetPosition().x, 1.0f);
	EXPECT_FLOAT_EQ(arc.GetPosition().y, 0.5f);
}

TEST(KnockbackTests, BottomlessFallsGiveUp) {
	const dpKnockback::GroundQuery nothing = [](const NiPoint3&) { return -100000.0f; };
	dpKnockback::Arc arc;
	ASSERT_TRUE(arc.Start(NiPoint3Constant::ZERO, NiPoint3(0.0f, 10.0f, 0.0f)));
	Fly(arc, nothing);
	EXPECT_FALSE(arc.IsActive());
	EXPECT_NEAR(arc.GetElapsed(), dpKnockback::MAX_AIRBORNE_TIME, 0.05f);
}
