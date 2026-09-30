#include "Matchmaking.h"

#include <algorithm>

#include <gtest/gtest.h>

// The chat server's activity lobbies (docs/Matchmaking.md)

using namespace Matchmaking;

namespace {
	constexpr LWOOBJID A = 0x1000000000000001LL;
	constexpr LWOOBJID B = 0x1000000000000002LL;
	constexpr LWOOBJID C = 0x1000000000000003LL;
	constexpr LWOOBJID D = 0x1000000000000004LL;
	constexpr LWOOBJID E = 0x1000000000000005LL;

	// Avant Gardens survival: one team of 1 to 4, a minute to wait, 3 seconds once everyone is ready
	ActivitySettings Survival() {
		ActivitySettings settings;
		settings.activityID = 5;
		settings.instanceMapID = 1101;
		settings.minTeams = 1;
		settings.maxTeams = 1;
		settings.minTeamSize = 2;
		settings.maxTeamSize = 4;
		settings.waitTime = 60.0f;
		settings.startDelay = 3.0f;
		return settings;
	}

	// A race: up to 6 teams of one, at least 2
	ActivitySettings Race() {
		ActivitySettings settings;
		settings.activityID = 42;
		settings.instanceMapID = 1203;
		settings.minTeams = 2;
		settings.maxTeams = 6;
		settings.minTeamSize = 1;
		settings.maxTeamSize = 1;
		settings.waitTime = 60.0f;
		settings.startDelay = 3.0f;
		return settings;
	}

	std::vector<Update> For(const std::vector<Update>& updates, LWOOBJID player) {
		std::vector<Update> result;
		for (const auto& update : updates) if (update.to == player) result.push_back(update);
		return result;
	}

	size_t Count(const std::vector<Update>& updates, eMatchUpdate type) {
		return std::ranges::count(updates, type, &Update::type);
	}
}

TEST(MatchmakingTests, SettingsCapacityAndMinimum) {
	EXPECT_EQ(Survival().Capacity(), 4u);
	EXPECT_EQ(Survival().Minimum(), 2u);
	EXPECT_EQ(Race().Capacity(), 6u);
	EXPECT_EQ(Race().Minimum(), 2u);
	ActivitySettings empty;
	EXPECT_EQ(empty.Capacity(), 1u);
	EXPECT_EQ(empty.Minimum(), 1u);
}

TEST(MatchmakingTests, UpdateTextsMatchLiveCaptures) {
	EXPECT_EQ(PlayerText(1152921510436607007LL), "player=9:1152921510436607007");
	EXPECT_EQ(PlayerAddedText({ 1152921510436607007LL, "GruntMonkey", "", false }), "player=9:1152921510436607007\nplayerName=0:GruntMonkey");
	// The racing car the client chose comes first, as an object ID
	EXPECT_EQ(PlayerAddedText({ 1152921510436607007LL, "GruntMonkey", "droppedItem=13:1152921510659010409", false }),
		"droppedItem=9:1152921510659010409\nplayer=9:1152921510436607007\nplayerName=0:GruntMonkey");
	EXPECT_EQ(PlayerAddedText({ 1, "x", "somethingElse=0:1\ndroppedItem=13:notanumber", false }), "player=9:1\nplayerName=0:x");
	EXPECT_EQ(TimeText(3.0f).rfind("time=3:", 0), 0u);
}

TEST(MatchmakingTests, JoinSendsTheJoinerThemselvesFirstThenTheOthers) {
	Lobbies lobbies;
	std::vector<Update> out;
	ASSERT_TRUE(lobbies.Join(A, "Alpha", "", Survival(), out));
	ASSERT_EQ(out.size(), 1u);
	EXPECT_EQ(out[0].to, A);
	EXPECT_EQ(out[0].type, eMatchUpdate::PLAYER_ADDED);
	EXPECT_EQ(out[0].data, "player=9:" + std::to_string(A) + "\nplayerName=0:Alpha");

	out.clear();
	ASSERT_TRUE(lobbies.SetReady(A, true, out));
	out.clear();
	ASSERT_TRUE(lobbies.Join(B, "Bravo", "", Survival(), out));
	// B: B added, A added, A ready. A: B added.
	const auto toB = For(out, B);
	ASSERT_EQ(toB.size(), 3u);
	EXPECT_EQ(toB[0].type, eMatchUpdate::PLAYER_ADDED);
	EXPECT_NE(toB[0].data.find("Bravo"), std::string::npos);
	EXPECT_EQ(toB[1].type, eMatchUpdate::PLAYER_ADDED);
	EXPECT_NE(toB[1].data.find("Alpha"), std::string::npos);
	EXPECT_EQ(toB[2].type, eMatchUpdate::PLAYER_READY);
	EXPECT_EQ(toB[2].data, PlayerText(A));
	const auto toA = For(out, A);
	ASSERT_EQ(toA.size(), 1u);
	EXPECT_EQ(toA[0].type, eMatchUpdate::PLAYER_ADDED);
	EXPECT_NE(toA[0].data.find("Bravo"), std::string::npos);

	// Joining again changes nothing
	out.clear();
	EXPECT_FALSE(lobbies.Join(B, "Bravo", "", Survival(), out));
	EXPECT_TRUE(out.empty());
	EXPECT_EQ(lobbies.GetLobbies().size(), 1u);
}

TEST(MatchmakingTests, CountdownStartsAtTheMinimumAndLateJoinersGetTheTimeLeft) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	out.clear();
	// Below the minimum nothing runs
	EXPECT_TRUE(lobbies.Tick(30.0f, out).empty());
	EXPECT_TRUE(out.empty());
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 60.0f);

	lobbies.Join(B, "Bravo", "", Survival(), out);
	EXPECT_EQ(Count(out, eMatchUpdate::PHASE_WAIT_READY), 0u);
	out.clear();
	EXPECT_TRUE(lobbies.Tick(10.0f, out).empty());
	// Everyone is told the countdown once
	EXPECT_EQ(Count(out, eMatchUpdate::PHASE_WAIT_READY), 2u);
	EXPECT_EQ(For(out, A)[0].data, TimeText(60.0f));
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 50.0f);
	out.clear();
	lobbies.Tick(1.0f, out);
	EXPECT_TRUE(out.empty());

	lobbies.Join(C, "Charlie", "", Survival(), out);
	const auto toC = For(out, C);
	ASSERT_FALSE(toC.empty());
	EXPECT_EQ(toC.back().type, eMatchUpdate::PHASE_WAIT_READY);
	EXPECT_EQ(toC.back().data, TimeText(49.0f));
}

TEST(MatchmakingTests, TimerRunsOutIntoOneMatchWithEveryoneInJoinOrder) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	lobbies.Join(C, "Charlie", "", Survival(), out);
	EXPECT_TRUE(lobbies.Tick(59.0f, out).empty());
	const auto matches = lobbies.Tick(1.5f, out);
	ASSERT_EQ(matches.size(), 1u);
	EXPECT_EQ(matches[0].players, (std::vector<LWOOBJID>{ A, B, C }));
	EXPECT_EQ(matches[0].settings.instanceMapID, 1101u);
	EXPECT_TRUE(lobbies.GetLobbies().empty());
	EXPECT_FALSE(lobbies.IsWaiting(A));
}

TEST(MatchmakingTests, EveryoneReadyCutsTheTimerToTheStartDelay) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	lobbies.Tick(1.0f, out);
	lobbies.SetReady(A, true, out);
	out.clear();
	lobbies.SetReady(B, true, out);
	EXPECT_EQ(Count(out, eMatchUpdate::PLAYER_READY), 2u);
	out.clear();
	EXPECT_TRUE(lobbies.Tick(1.0f, out).empty());
	EXPECT_EQ(Count(out, eMatchUpdate::PHASE_WAIT_START), 2u);
	EXPECT_EQ(For(out, A)[0].data, TimeText(3.0f));
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 3.0f);
	EXPECT_TRUE(lobbies.Tick(2.0f, out).empty());
	EXPECT_EQ(lobbies.Tick(1.0f, out).size(), 1u);
}

TEST(MatchmakingTests, ReadyBelowTheMinimumDoesNotCutTheTimer) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.SetReady(A, true, out);
	lobbies.Tick(1.0f, out);
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 60.0f);
}

TEST(MatchmakingTests, UnreadyIsBroadcast) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	out.clear();
	lobbies.SetReady(B, false, out);
	EXPECT_EQ(Count(out, eMatchUpdate::PLAYER_NOT_READY), 2u);
	EXPECT_FALSE(lobbies.SetReady(C, true, out));
}

TEST(MatchmakingTests, LeaveTellsEveryoneIncludingTheLeaverAndEmptyLobbiesGo) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	out.clear();
	ASSERT_TRUE(lobbies.Leave(B, out));
	ASSERT_EQ(out.size(), 2u);
	EXPECT_EQ(Count(out, eMatchUpdate::PLAYER_REMOVED), 2u);
	EXPECT_EQ(out[0].data, PlayerText(B));
	EXPECT_FALSE(lobbies.IsWaiting(B));
	EXPECT_FALSE(lobbies.Leave(B, out));
	ASSERT_TRUE(lobbies.Leave(A, out));
	EXPECT_TRUE(lobbies.GetLobbies().empty());
}

TEST(MatchmakingTests, CountdownPausesBelowTheMinimumWithoutStartingOver) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	lobbies.Tick(20.0f, out);
	lobbies.Leave(B, out);
	lobbies.Tick(20.0f, out);
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 40.0f);
	out.clear();
	lobbies.Join(C, "Charlie", "", Survival(), out);
	EXPECT_EQ(For(out, C).back().data, TimeText(40.0f));
}

TEST(MatchmakingTests, FullLobbiesOverflowIntoANewOne) {
	Lobbies lobbies;
	std::vector<Update> out;
	for (const auto player : { A, B, C, D }) lobbies.Join(player, "p", "", Survival(), out);
	EXPECT_EQ(lobbies.GetLobbies().size(), 1u);
	lobbies.Join(E, "e", "", Survival(), out);
	EXPECT_EQ(lobbies.GetLobbies().size(), 2u);
	EXPECT_NE(lobbies.Find(A), lobbies.Find(E));

	// A place in the first lobby is taken before the second one
	lobbies.Leave(B, out);
	constexpr LWOOBJID F = 0x1000000000000006LL;
	lobbies.Join(F, "f", "", Survival(), out);
	EXPECT_EQ(lobbies.Find(F), lobbies.Find(A));
}

TEST(MatchmakingTests, RacesHoldSixSingleTeams) {
	Lobbies lobbies;
	std::vector<Update> out;
	for (LWOOBJID player = 1; player <= 6; player++) lobbies.Join(player, "p", "", Race(), out);
	EXPECT_EQ(lobbies.GetLobbies().size(), 1u);
	lobbies.Join(7, "p", "", Race(), out);
	EXPECT_EQ(lobbies.GetLobbies().size(), 2u);
}

TEST(MatchmakingTests, DifferentActivitiesAndZonesWaitApart) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Race(), out);
	auto otherZone = Survival();
	otherZone.instanceMapID = 1102;
	lobbies.Join(C, "Charlie", "", otherZone, out);
	EXPECT_EQ(lobbies.GetLobbies().size(), 3u);

	// Joining another activity leaves the first lobby
	out.clear();
	ASSERT_TRUE(lobbies.Join(A, "Alpha", "", Race(), out));
	EXPECT_EQ(Count(out, eMatchUpdate::PLAYER_REMOVED), 1u);
	EXPECT_EQ(lobbies.Find(A), lobbies.Find(B));
	EXPECT_EQ(lobbies.GetLobbies().size(), 2u);
}

TEST(MatchmakingTests, SoloActivityStartsAfterTheWaitAlone) {
	// Solo racing (the world's override): one player is enough
	auto solo = Race();
	solo.minTeams = 1;
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", solo, out);
	lobbies.SetReady(A, true, out);
	EXPECT_TRUE(lobbies.Tick(0.5f, out).empty());
	EXPECT_FLOAT_EQ(lobbies.Find(A)->timer, 3.0f);
	const auto matches = lobbies.Tick(3.0f, out);
	ASSERT_EQ(matches.size(), 1u);
	EXPECT_EQ(matches[0].players, std::vector<LWOOBJID>{ A });
}

TEST(MatchmakingTests, SeveralLobbiesStartInTheSameTick) {
	Lobbies lobbies;
	std::vector<Update> out;
	lobbies.Join(A, "Alpha", "", Survival(), out);
	lobbies.Join(B, "Bravo", "", Survival(), out);
	lobbies.Join(C, "Charlie", "", Race(), out);
	lobbies.Join(D, "Delta", "", Race(), out);
	const auto matches = lobbies.Tick(61.0f, out);
	EXPECT_EQ(matches.size(), 2u);
	EXPECT_TRUE(lobbies.GetLobbies().empty());
}
