#include "GameMessages.h"
#include "RacingControlComponent.h"
#include "ZoneFile.h"

#include <gtest/gtest.h>

namespace {
	// A straight track along +Z with planes every 10 units, all facing along the track
	std::vector<PathWaypoint> StraightTrack() {
		std::vector<PathWaypoint> waypoints(5);
		for (size_t i = 0; i < waypoints.size(); i++) waypoints[i].position = NiPoint3(0.0f, 0.0f, 10.0f * static_cast<float>(i));
		return waypoints;
	}
}

TEST(RacingWrongWayTests, DrivingForwardMovesThePlanes) {
	const auto track = StraightTrack();
	RacingPlayerInfo player{};
	RacingControlComponent::StepResetPlanes(player, track, NiPoint3(0.0f, 0.0f, 5.0f));
	EXPECT_EQ(player.lastPlane, 0u);
	EXPECT_EQ(player.upcomingPlane, 1u);
	RacingControlComponent::StepResetPlanes(player, track, NiPoint3(0.0f, 0.0f, 25.0f));
	EXPECT_EQ(player.lastPlane, 2u);
	EXPECT_EQ(player.upcomingPlane, 3u);
	EXPECT_EQ(player.wrongWayCount, 0u);
	EXPECT_LT(player.wrongWayTime, 0.0f);
}

TEST(RacingWrongWayTests, TheSecondPlaneBackStartsTheCountdown) {
	const auto track = StraightTrack();
	RacingPlayerInfo player{};
	player.lastPlane = 3;
	player.upcomingPlane = 4;
	RacingControlComponent::StepResetPlanes(player, track, NiPoint3(0.0f, 0.0f, 25.0f));
	EXPECT_EQ(player.wrongWayCount, 1u);
	EXPECT_LT(player.wrongWayTime, 0.0f);
	RacingControlComponent::StepResetPlanes(player, track, NiPoint3(0.0f, 0.0f, 15.0f));
	EXPECT_EQ(player.wrongWayCount, 2u);
	EXPECT_EQ(player.lastPlane, 1u);
	EXPECT_EQ(player.upcomingPlane, 2u);
	EXPECT_FLOAT_EQ(player.wrongWayTime, 6.0f);

	// Turning around and going through a plane again ends it
	RacingControlComponent::StepResetPlanes(player, track, NiPoint3(0.0f, 0.0f, 21.0f));
	EXPECT_EQ(player.wrongWayCount, 0u);
	EXPECT_LT(player.wrongWayTime, 0.0f);
	EXPECT_EQ(player.lastPlane, 2u);
}
