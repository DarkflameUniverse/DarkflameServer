#include <gtest/gtest.h>

#include <random>

#include "LiveOpsRules.h"

using namespace LiveOpsRules;

namespace {
	std::vector<NiPoint3> Line(int count, float step) {
		std::vector<NiPoint3> points;
		for (int i = 0; i < count; i++) points.emplace_back(static_cast<float>(i) * step, 0.0f, 0.0f);
		return points;
	}

	float MinDistance(const std::vector<NiPoint3>& points) {
		float best = 1e9f;
		for (size_t i = 0; i < points.size(); i++) {
			for (size_t j = i + 1; j < points.size(); j++) best = std::min(best, NiPoint3::Distance(points[i], points[j]));
		}
		return best;
	}
}

TEST(LiveOpsRulesTests, EventTypeNamesRoundTrip) {
	for (const auto type : { eEventType::TREASURE_HUNT, eEventType::BONUS, eEventType::INVASION, eEventType::CELEBRATION }) {
		EXPECT_EQ(ParseType(TypeName(type)), type);
	}
	EXPECT_FALSE(ParseType("party").has_value());
	EXPECT_FALSE(NeedsZone(eEventType::BONUS));
	EXPECT_TRUE(NeedsZone(eEventType::TREASURE_HUNT));
}

TEST(LiveOpsRulesTests, PlacementOnlyUsesCandidatesAndKeepsThemApart) {
	std::mt19937 rng(42);
	const auto candidates = Line(20, 10.0f); // 10 apart
	const auto chosen = ChoosePositions(candidates, { .count = 5, .minSpacing = 25.0f }, rng);
	ASSERT_EQ(chosen.size(), 5u);
	EXPECT_GE(MinDistance(chosen), 25.0f);
	for (const auto& point : chosen) EXPECT_NE(std::find(candidates.begin(), candidates.end(), point), candidates.end());
}

TEST(LiveOpsRulesTests, PlacementRelaxesSpacingRatherThanGivingUp) {
	std::mt19937 rng(1);
	const auto chosen = ChoosePositions(Line(6, 2.0f), { .count = 6, .minSpacing = 50.0f }, rng);
	EXPECT_EQ(chosen.size(), 6u);
}

TEST(LiveOpsRulesTests, PlacementNeverInventsPositionsWithoutReuse) {
	std::mt19937 rng(7);
	EXPECT_EQ(ChoosePositions(Line(3, 10.0f), { .count = 10, .minSpacing = 1.0f }, rng).size(), 3u);
	EXPECT_TRUE(ChoosePositions({}, { .count = 10 }, rng).empty());
	EXPECT_TRUE(ChoosePositions(Line(3, 10.0f), { .count = 0 }, rng).empty());
}

TEST(LiveOpsRulesTests, PlacementAroundACenterCyclesWhenReusing) {
	std::mt19937 rng(3);
	const auto chosen = ChoosePositions(Line(20, 10.0f), { .count = 8, .minSpacing = 1.0f, .center = NiPoint3(0.0f, 0.0f, 0.0f), .radius = 15.0f, .reuse = true }, rng);
	ASSERT_EQ(chosen.size(), 8u);
	for (const auto& point : chosen) EXPECT_LE(point.x, 15.0f); // only the two within the radius, used again
}

TEST(LiveOpsRulesTests, MultipliersApplyOnlyInTheirWindowAndDontStack) {
	const std::vector<BonusWindow> windows{
		{ 100, 200, { 2.0f, 1.0f, 1.0f } },
		{ 150, 300, { 3.0f, 1.5f, 1.0f } },
		{ 0, 1000, { 50.0f, 0.5f, 1.0f } }, // clamped to 10, and below 1 means no change
	};
	EXPECT_EQ(ActiveMultipliers({}, 120), Multipliers{});
	EXPECT_FALSE(ActiveMultipliers({ windows[0] }, 99).Any());
	EXPECT_EQ(ActiveMultipliers({ windows[0] }, 100).coins, 2.0f);
	EXPECT_FALSE(ActiveMultipliers({ windows[0] }, 200).Any()); // the end is exclusive
	const auto both = ActiveMultipliers({ windows[0], windows[1] }, 160);
	EXPECT_EQ(both.coins, 3.0f);
	EXPECT_EQ(both.uscore, 1.5f);
	EXPECT_EQ(ActiveMultipliers({ windows[2] }, 500).coins, MAX_MULTIPLIER);
	EXPECT_EQ(ActiveMultipliers({ windows[2] }, 500).uscore, 1.0f);
}

TEST(LiveOpsRulesTests, ScalingRoundsSaturatesAndLeavesNoBonusAlone) {
	EXPECT_EQ(Scale<uint32_t>(15, 1.0f), 15u);
	EXPECT_EQ(Scale<uint32_t>(15, 2.0f), 30u);
	EXPECT_EQ(Scale<uint32_t>(15, 1.5f), 23u);
	EXPECT_EQ(Scale<uint32_t>(0, 3.0f), 0u);
	EXPECT_EQ(Scale<uint32_t>(4000000000u, 2.0f), UINT32_MAX);
	EXPECT_EQ(Scale<int64_t>(-5, 2.0f), -5);
	EXPECT_FLOAT_EQ(ScaleChance(0.3f, 2.0f), 0.6f);
	EXPECT_FLOAT_EQ(ScaleChance(0.7f, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(ScaleChance(0.7f, 1.0f), 0.7f);
}

TEST(LiveOpsRulesTests, WavesComeAtTheStartAndThenEveryInterval) {
	EXPECT_EQ(WavesDue(100, 60, 3, 99), 0u);
	EXPECT_EQ(WavesDue(100, 60, 3, 100), 1u);
	EXPECT_EQ(WavesDue(100, 60, 3, 159), 1u);
	EXPECT_EQ(WavesDue(100, 60, 3, 160), 2u);
	EXPECT_EQ(WavesDue(100, 60, 3, 10000), 3u);
	EXPECT_EQ(WavesDue(100, 60, 0, 10000), 0u);
}

TEST(LiveOpsRulesTests, ChallengePhaseAndPercent) {
	EXPECT_EQ(Phase(0, 100, 200, 50), ePhase::SCHEDULED);
	EXPECT_EQ(Phase(0, 100, 200, 150), ePhase::ACTIVE);
	EXPECT_EQ(Phase(0, 100, 200, 200), ePhase::EXPIRED);
	EXPECT_EQ(Phase(1, 100, 200, 150), ePhase::COMPLETED);
	EXPECT_EQ(Phase(3, 100, 200, 150), ePhase::CANCELLED);
	EXPECT_EQ(Percent(0, 1000), 0);
	EXPECT_EQ(Percent(250, 1000), 25);
	EXPECT_EQ(Percent(999, 1000), 99); // 100 only when reached
	EXPECT_EQ(Percent(1000, 1000), 100);
	EXPECT_EQ(Percent(5000, 1000), 100);
}

TEST(LiveOpsRulesTests, MilestonesAnnounceTheHighestNewOneOnce) {
	const auto milestones = ParseMilestones("75, 25,50,abc,50,0,101,100");
	EXPECT_EQ(milestones, (std::vector<uint8_t>{ 25, 50, 75, 100 }));
	EXPECT_EQ(ParseMilestones(""), DefaultMilestones());
	EXPECT_FALSE(NextMilestone(milestones, 0, 200, 1000).has_value());
	EXPECT_EQ(NextMilestone(milestones, 0, 250, 1000), 25);
	EXPECT_FALSE(NextMilestone(milestones, 25, 300, 1000).has_value());
	EXPECT_EQ(NextMilestone(milestones, 25, 800, 1000), 75); // jumped past 50: one message
	EXPECT_EQ(NextMilestone(milestones, 75, 1000, 1000), 100);
	EXPECT_FALSE(NextMilestone(milestones, 100, 1000, 1000).has_value());
}

TEST(LiveOpsRulesTests, RewardsGoToThoseWhoContributedEnoughOnce) {
	EXPECT_FALSE(Eligible(0, 1));
	EXPECT_TRUE(Eligible(1, 1));
	EXPECT_TRUE(Eligible(1, 0)); // a minimum below 1 still needs something
	EXPECT_FALSE(Eligible(9, 10));
	const std::vector<std::pair<int64_t, int64_t>> contributions{ { 1, 5 }, { 2, 50 }, { 3, 10 }, { 4, 0 }, { 5, 10 } };
	const auto recipients = RewardRecipients(contributions, 10, std::vector<int64_t>{ 5 });
	ASSERT_EQ(recipients.size(), 2u);
	EXPECT_EQ(recipients[0].first, 2); // highest first
	EXPECT_EQ(recipients[1].first, 3);
}

TEST(LiveOpsRulesTests, ZonesLimitWhatCounts) {
	EXPECT_TRUE(CountsInZone({}, 1100));
	EXPECT_TRUE(CountsInZone({ 1100, 1200 }, 1200));
	EXPECT_FALSE(CountsInZone({ 1100 }, 1200));
}

TEST(LiveOpsRulesTests, EndAnnouncementDueOnceAndOnlyWhileRecent) {
	constexpr int64_t now = 1'000'000;
	EXPECT_TRUE(EndAnnouncementDue(1, 0, now - 4, now));                            // completed during a restart
	EXPECT_TRUE(EndAnnouncementDue(2, 0, now - 60, now));                           // expired
	EXPECT_FALSE(EndAnnouncementDue(1, now - 4, now - 4, now));                     // announced already
	EXPECT_FALSE(EndAnnouncementDue(0, 0, now - 4, now));                           // still open
	EXPECT_FALSE(EndAnnouncementDue(3, 0, now - 4, now));                           // cancelled: never announced
	EXPECT_FALSE(EndAnnouncementDue(1, 0, now - END_ANNOUNCEMENT_WINDOW - 1, now)); // too long ago to be news
	EXPECT_FALSE(EndAnnouncementDue(1, 0, 0, now));                                 // no end time
}
