#include <gtest/gtest.h>

#include "HotPropertySlots.h"
#include "PropertyRentRules.h"

using namespace PropertyRentRules;

TEST(PropertyRentRulesTests, TemplateRates) {
	// Block Yard: free
	EXPECT_EQ(TemplateRate(0, 1, 0), std::nullopt);
	// Property worlds: 1 month
	EXPECT_EQ(TemplateRate(1000, 1, 6), (Rate{ 1000, 30 * DAY }));
	// The test templates: 7 days
	EXPECT_EQ(TemplateRate(10000, 7, 1), (Rate{ 10000, 7 * DAY }));
	EXPECT_EQ(TemplateRate(500, 0, 6), std::nullopt);
}

TEST(PropertyRentRulesTests, OverridesWin) {
	const auto month = TemplateRate(1000, 1, 6);
	EXPECT_EQ(Resolve(month, std::nullopt, std::nullopt), month);
	EXPECT_EQ(Resolve(month, 0, 0), std::nullopt);                          // made free
	EXPECT_EQ(Resolve(month, 250, 0), (Rate{ 250, 30 * DAY }));             // the template's period
	EXPECT_EQ(Resolve(month, 250, 7), (Rate{ 250, 7 * DAY }));
	EXPECT_EQ(Resolve(std::nullopt, 100, 0), (Rate{ 100, 30 * DAY }));      // a free world given a price
}

TEST(PropertyRentRulesTests, Charging) {
	const Rate rate{ 1000, 30 * DAY };
	const int64_t now = 1'800'000'000, grace = 3 * DAY;
	// Free and not yet due
	EXPECT_EQ(Decide(std::nullopt, 0, now, 0, grace).outcome, eOutcome::NOT_DUE);
	EXPECT_EQ(Decide(rate, now + 10, now, 5000, grace).outcome, eOutcome::NOT_DUE);
	// Never charged: due now; paying moves it one period on
	EXPECT_EQ(Decide(rate, 0, now, 5000, grace), (Decision{ eOutcome::PAID, 1000, now + 30 * DAY, false }));
	// Long overdue: only one period is charged, counted from now
	EXPECT_EQ(Decide(rate, now - 90 * DAY, now, 5000, grace), (Decision{ eOutcome::PAID, 1000, now + 30 * DAY, false }));
	// Can't pay: the grace period starts at the due date (now, when never charged)
	EXPECT_EQ(Decide(rate, 0, now, 999, grace), (Decision{ eOutcome::UNPAID, 0, now, false }));
	EXPECT_EQ(Decide(rate, now - DAY, now, 0, grace), (Decision{ eOutcome::UNPAID, 0, now - DAY, false }));
	EXPECT_EQ(Decide(rate, now - 3 * DAY, now, 0, grace), (Decision{ eOutcome::UNPAID, 0, now - 3 * DAY, true }));
	EXPECT_TRUE(Decide(rate, 0, now, 0, 0).overdue);
}

TEST(PropertyRentRulesTests, Overdue) {
	const Rate rate{ 1000, 30 * DAY };
	const int64_t now = 1'800'000'000, grace = 3 * DAY;
	EXPECT_FALSE(IsOverdue(std::nullopt, now - 90 * DAY, now, grace));
	EXPECT_FALSE(IsOverdue(rate, 0, now, grace));              // never charged
	EXPECT_FALSE(IsOverdue(rate, now + DAY, now, grace));      // paid
	EXPECT_FALSE(IsOverdue(rate, now - DAY, now, grace));      // in the grace period
	EXPECT_TRUE(IsOverdue(rate, now - 3 * DAY, now, grace));
}

TEST(PropertyRentRulesTests, WorldTemplate) {
	using namespace HotPropertySlots;
	const std::vector<TemplateRow> rows{ { 25218, 58001, "AGSmallProperty", 1, 1, 0 }, { 25166, 1150, "AGSmallProperty", 0, 1, 0 },
		{ 25168, 1151, "AGMedProperty", 1000, 1, 6 }, { 25250, 1150, "Other", 5, 1, 1 } };
	const std::vector<EntranceRow> entrances{ { 1150, "AGSmallProperty" }, { 1151, "AGMedProperty" } };
	ASSERT_NE(WorldTemplate(rows, entrances, 1150), nullptr);
	EXPECT_EQ(WorldTemplate(rows, entrances, 1150)->id, 25166u);
	EXPECT_EQ(WorldTemplate(rows, entrances, 1151)->minimumPrice, 1000);
	EXPECT_EQ(WorldTemplate(rows, entrances, 58001)->id, 25218u); // no entrance: its lowest row
	EXPECT_EQ(WorldTemplate(rows, entrances, 1), nullptr);
}
