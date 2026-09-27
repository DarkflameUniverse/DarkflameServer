#include <gtest/gtest.h>

#include "PropertyReputationRules.h"

using namespace PropertyReputationRules;

namespace {
	constexpr int64_t START = 1'800'000'000;
	constexpr uint32_t DAY = 20000;

	// Minutes of one visit (active unless given); returns the points given
	int64_t Minutes(Visit& visit, const Params& params, int minutes, int64_t& propertyToday, bool active = true, int32_t perMinute = 1) {
		int64_t total = 0;
		for (int minute = 1; minute <= minutes; minute++) total += OnMinute(visit, params, perMinute, START + minute * 60, DAY, active, propertyToday);
		return total;
	}

	Visit NewVisit(double repeatFactor = 1.0, int64_t visitorToday = 0) {
		Visit visit;
		visit.enteredAt = START;
		visit.day = DAY;
		visit.repeatFactor = repeatFactor;
		visit.visitorToday = visitorToday;
		return visit;
	}
}

TEST(PropertyReputationRulesTests, OnlyOtherPeopleCount) {
	Params params;
	const std::set<uint32_t> linked{ 7 };
	EXPECT_TRUE(Eligible(2, 1, linked, false, params));
	EXPECT_FALSE(Eligible(1, 1, linked, false, params));  // the owner's account: any of their characters
	EXPECT_FALSE(Eligible(7, 1, linked, false, params));  // shares a play key, email or address with the owner
	EXPECT_FALSE(Eligible(2, 1, linked, true, params));   // staff
	EXPECT_FALSE(Eligible(0, 1, linked, false, params));
	params.ignoreLinked = false;
	params.ignoreStaff = false;
	EXPECT_TRUE(Eligible(7, 1, linked, false, params));
	EXPECT_TRUE(Eligible(2, 1, linked, true, params));
}

TEST(PropertyReputationRulesTests, ShortVisitsEarnNothing) {
	Params params;
	auto visit = NewVisit();
	int64_t today = 0;
	// The first two minutes are before the minimum visit
	EXPECT_EQ(OnMinute(visit, params, 1, START + 60, DAY, true, today), 0);
	EXPECT_EQ(OnMinute(visit, params, 1, START + 119, DAY, true, today), 0);
	EXPECT_EQ(OnMinute(visit, params, 1, START + 120, DAY, true, today), 1);
	EXPECT_EQ(visit.creditedMinutes, 1);
}

TEST(PropertyReputationRulesTests, OnePointPerActiveMinuteLikeLive) {
	Params params;
	auto visit = NewVisit();
	int64_t today = 0;
	EXPECT_EQ(Minutes(visit, params, 12, today), 11); // minutes 2..12
	EXPECT_EQ(today, 11);
}

TEST(PropertyReputationRulesTests, IdleVisitorsEarnNothing) {
	Params params;
	auto visit = NewVisit();
	int64_t today = 0;
	EXPECT_EQ(Minutes(visit, params, 20, today, false), 0);
	EXPECT_EQ(visit.creditedMinutes, 0);
	params.requireActivity = false;
	EXPECT_EQ(Minutes(visit, params, 20, today, false), 19);
}

TEST(PropertyReputationRulesTests, VisitLengthIsCapped) {
	Params params;
	params.visitorDailyCap = 1000;
	auto visit = NewVisit();
	int64_t today = 0;
	EXPECT_EQ(Minutes(visit, params, 120, today), params.maxMinutesPerVisit);
}

TEST(PropertyReputationRulesTests, VisitorDailyCap) {
	Params params;
	params.maxMinutesPerVisit = 1000;
	auto visit = NewVisit(1.0, 25); // already gave 25 today in earlier visits
	int64_t today = 25;
	EXPECT_EQ(Minutes(visit, params, 60, today), 5);
	EXPECT_EQ(visit.visitorToday, 30);
	// A new day starts the count again
	EXPECT_EQ(OnMinute(visit, params, 1, START + 3600, DAY + 1, true, today), 1);
	EXPECT_EQ(visit.visitorToday, 1);
}

TEST(PropertyReputationRulesTests, PropertyDailyCap) {
	Params params;
	auto visit = NewVisit();
	int64_t today = params.propertyDailyCap - 3;
	EXPECT_EQ(Minutes(visit, params, 30, today), 3);
	EXPECT_EQ(today, params.propertyDailyCap);
}

TEST(PropertyReputationRulesTests, RepeatVisitorsCountLess) {
	EXPECT_DOUBLE_EQ(RepeatFactor(0, 0.5), 1.0);
	EXPECT_DOUBLE_EQ(RepeatFactor(2, 0.5), 0.5);
	EXPECT_DOUBLE_EQ(RepeatFactor(8, 0.5), 0.2);
	EXPECT_DOUBLE_EQ(RepeatFactor(8, 0.0), 1.0);

	Params params;
	// Came on 2 recent days: half as much, fractions carried between minutes
	auto visit = NewVisit(RepeatFactor(2, 0.5));
	int64_t today = 0;
	EXPECT_EQ(Minutes(visit, params, 21, today), 10); // 20 counted minutes x 0.5
}

TEST(PropertyReputationRulesTests, ManyVisitorsBeatOneFarmer) {
	Params params;
	int64_t today = 0;
	// One account visiting every day for a week, as long as it can
	int64_t farmer = 0;
	for (uint32_t d = 0; d < 7; d++) {
		auto visit = NewVisit(RepeatFactor(d, params.repeatFalloff));
		farmer += Minutes(visit, params, 60, today);
	}
	// Seven different people once each, 20 minutes
	int64_t visitors = 0;
	int64_t today2 = 0;
	for (int i = 0; i < 7; i++) {
		auto visit = NewVisit();
		visitors += Minutes(visit, params, 21, today2);
	}
	EXPECT_LT(farmer, visitors);
	EXPECT_EQ(visitors, 140);
}

TEST(PropertyReputationRulesTests, MultiplierAndTemplateRate) {
	Params params;
	params.multiplier = 2.5;
	params.visitorDailyCap = 1000;
	auto visit = NewVisit();
	int64_t today = 0;
	EXPECT_EQ(Minutes(visit, params, 5, today), 10); // 4 minutes x 2.5
	params.multiplier = 0;
	EXPECT_EQ(Minutes(visit, params, 5, today), 0);
	auto other = NewVisit();
	params.multiplier = 1;
	EXPECT_EQ(Minutes(other, params, 5, today, true, 0), 0); // a template without reputationPerMinute
}

TEST(PropertyReputationRulesTests, Movement) {
	EXPECT_FALSE(Moved({ 0, 0, 0 }, { 1, 0, 1 }));
	EXPECT_TRUE(Moved({ 0, 0, 0 }, { 2, 0, 0 }));
	EXPECT_TRUE(Moved({ 0, 0, 0 }, { 0, 5, 0 }));
}
