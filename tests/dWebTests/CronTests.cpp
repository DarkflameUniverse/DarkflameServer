#include <gtest/gtest.h>

#include "Cron.h"

namespace {
	// 2026-09-25 (a Friday) at hh:mm UTC
	int64_t At(int hour, int minute, int dayOffset = 0) {
		return (Cron::Detail::DaysFromCivil(2026, 9, 25) + dayOffset) * 86400 + hour * 3600 + minute * 60;
	}

	int64_t NextOf(const std::string& text, int64_t after) {
		std::string error;
		const auto schedule = Cron::Parse(text, error);
		EXPECT_TRUE(schedule.has_value()) << text << ": " << error;
		if (!schedule) return -1;
		return Cron::Next(*schedule, after).value_or(-1);
	}
}

TEST(CronTests, CivilDatesRoundTrip) {
	EXPECT_EQ(Cron::Detail::DaysFromCivil(1970, 1, 1), 0);
	EXPECT_EQ(Cron::Detail::DaysFromCivil(2000, 3, 1), 11017);
	const auto date = Cron::Detail::CivilFromDays(Cron::Detail::DaysFromCivil(2028, 2, 29));
	EXPECT_EQ(date.year, 2028);
	EXPECT_EQ(date.month, 2u);
	EXPECT_EQ(date.day, 29u);
}

TEST(CronTests, DailyAtFixedTime) {
	EXPECT_EQ(NextOf("15 0 * * *", At(0, 0)), At(0, 15));
	EXPECT_EQ(NextOf("15 0 * * *", At(0, 15)), At(0, 15, 1)); // strictly after
	EXPECT_EQ(NextOf("15 0 * * *", At(12, 0)), At(0, 15, 1));
	EXPECT_EQ(NextOf("@daily", At(12, 0)), At(0, 0, 1));
}

TEST(CronTests, StepsListsAndRanges) {
	EXPECT_EQ(NextOf("*/15 * * * *", At(10, 1)), At(10, 15));
	EXPECT_EQ(NextOf("*/15 * * * *", At(10, 59)), At(11, 0));
	EXPECT_EQ(NextOf("0 */6 * * *", At(7, 0)), At(12, 0));
	EXPECT_EQ(NextOf("5,35 9-10 * * *", At(10, 40)), At(9, 5, 1));
	EXPECT_EQ(NextOf("10/20 * * * *", At(10, 11)), At(10, 30));
	EXPECT_EQ(NextOf("@hourly", At(10, 30)), At(11, 0));
}

TEST(CronTests, Weekdays) {
	// 2026-09-25 is a Friday; the next Sunday is two days later
	EXPECT_EQ(NextOf("0 3 * * sun", At(12, 0)), At(3, 0, 2));
	EXPECT_EQ(NextOf("0 3 * * 7", At(12, 0)), At(3, 0, 2));
	EXPECT_EQ(NextOf("0 3 * * mon-fri", At(2, 0)), At(3, 0));
	EXPECT_EQ(NextOf("0 3 * * MON-FRI", At(4, 0)), At(3, 0, 3)); // Saturday and Sunday skipped
	EXPECT_EQ(NextOf("@weekly", At(12, 0)), At(0, 0, 2));
}

TEST(CronTests, MonthsAndDaysOfMonth) {
	EXPECT_EQ(NextOf("@monthly", At(12, 0)), Cron::Detail::DaysFromCivil(2026, 10, 1) * 86400);
	EXPECT_EQ(NextOf("0 0 1 jan *", At(12, 0)), Cron::Detail::DaysFromCivil(2027, 1, 1) * 86400);
	EXPECT_EQ(NextOf("0 0 29 2 *", At(12, 0)), Cron::Detail::DaysFromCivil(2028, 2, 29) * 86400);
	// Both day fields restricted: either matches (the 1st, or any Sunday)
	EXPECT_EQ(NextOf("0 0 1 * sun", At(12, 0)), At(0, 0, 2));
}

TEST(CronTests, NeverFires) {
	const auto schedule = Cron::Parse("0 0 31 2 *");
	ASSERT_TRUE(schedule.has_value());
	EXPECT_FALSE(Cron::Next(*schedule, At(0, 0)).has_value());
}

TEST(CronTests, Intervals) {
	EXPECT_EQ(NextOf("@every 10m", 1000), 1600);
	EXPECT_EQ(NextOf("@every 30s", 1000), 1030);
	EXPECT_EQ(NextOf("@every 2h", 0), 7200);
	EXPECT_EQ(NextOf("@every 1d", 0), 86400);
	EXPECT_FALSE(Cron::Parse("@every 5s").has_value());
	EXPECT_FALSE(Cron::Parse("@every 10x").has_value());
	EXPECT_FALSE(Cron::Parse("@every").has_value());
}

TEST(CronTests, RejectsBadSchedules) {
	for (const auto* bad : { "", "* * * *", "* * * * * *", "60 * * * *", "* 24 * * *", "* * 0 * *", "* * * 13 *", "* * * * 8",
		"5-1 * * * *", "*/0 * * * *", "a * * * *", "1,,2 * * * *", "@sometimes", "-1 * * * *" }) {
		std::string error;
		EXPECT_FALSE(Cron::Parse(bad, error).has_value()) << bad;
		EXPECT_FALSE(error.empty()) << bad;
	}
}
