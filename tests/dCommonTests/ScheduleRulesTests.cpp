#include <gtest/gtest.h>

#include "CivilDate.h"
#include "ScheduleRules.h"

using namespace ScheduleRules;

namespace {
	int64_t Utc(int64_t year, unsigned month, unsigned day, int hour = 0, int minute = 0) {
		return CivilDate::DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60;
	}

	Schedule Parse(const std::string& json) {
		std::string error;
		const auto schedule = ParseSchedule(json, error);
		EXPECT_TRUE(schedule) << json << ": " << error;
		return schedule.value_or(Schedule{});
	}

	std::string ErrorOf(const std::string& json) {
		std::string error;
		EXPECT_FALSE(ParseSchedule(json, error)) << json;
		return error;
	}

}

TEST(ScheduleRulesTests, YearlyRangeIsWholeDays) {
	const auto october = Parse(R"({"rules": [{"type": "yearly", "from": "10-01", "to": "10-31"}]})");
	EXPECT_FALSE(Active(october, Utc(2026, 9, 30, 23, 59)));
	EXPECT_TRUE(Active(october, Utc(2026, 10, 1)));
	EXPECT_TRUE(Active(october, Utc(2026, 10, 31, 23, 59)));
	EXPECT_FALSE(Active(october, Utc(2026, 11, 1)));
	EXPECT_TRUE(Active(october, Utc(2031, 10, 15, 12)));
	EXPECT_EQ(NextChange(october, Utc(2026, 9, 26, 12)), Utc(2026, 10, 1));
	EXPECT_EQ(NextChange(october, Utc(2026, 10, 1)), Utc(2026, 11, 1));
	EXPECT_EQ(NextChange(october, Utc(2026, 11, 1)), Utc(2027, 10, 1));
}

TEST(ScheduleRulesTests, YearlyRangeWrapsTheYearEnd) {
	const auto holidays = Parse(R"({"rules": [{"type": "yearly", "from": "12-20", "to": "01-02"}]})");
	EXPECT_FALSE(Active(holidays, Utc(2026, 12, 19, 23, 59)));
	EXPECT_TRUE(Active(holidays, Utc(2026, 12, 20)));
	EXPECT_TRUE(Active(holidays, Utc(2026, 12, 31, 23, 59)));
	EXPECT_TRUE(Active(holidays, Utc(2027, 1, 1)));
	EXPECT_TRUE(Active(holidays, Utc(2027, 1, 2, 23, 59)));
	EXPECT_FALSE(Active(holidays, Utc(2027, 1, 3)));
	EXPECT_FALSE(Active(holidays, Utc(2027, 6, 1)));
	EXPECT_EQ(NextChange(holidays, Utc(2026, 12, 25)), Utc(2027, 1, 3));
	const auto windows = Windows(holidays, Utc(2026, 1, 1), Utc(2028, 1, 1));
	ASSERT_EQ(windows.size(), 3u); // the end of the last one, all of this one, the start of the next
	EXPECT_EQ(windows[0].start, Utc(2026, 1, 1));
	EXPECT_EQ(windows[0].end, Utc(2026, 1, 3));
	EXPECT_EQ(windows[1].start, Utc(2026, 12, 20));
	EXPECT_EQ(windows[1].end, Utc(2027, 1, 3));
	EXPECT_EQ(windows[2].start, Utc(2027, 12, 20));
	EXPECT_EQ(windows[2].end, Utc(2028, 1, 1));
}

TEST(ScheduleRulesTests, LeapDay) {
	const auto leap = Parse(R"({"rules": [{"type": "yearly", "from": "02-29", "to": "02-29"}]})");
	EXPECT_TRUE(Active(leap, Utc(2028, 2, 29, 12)));
	EXPECT_FALSE(Active(leap, Utc(2028, 3, 1)));
	EXPECT_FALSE(Active(leap, Utc(2027, 2, 28)));
	EXPECT_EQ(NextChange(leap, Utc(2026, 9, 26)), Utc(2028, 2, 29));
}

TEST(ScheduleRulesTests, OneOffDates) {
	// A date alone as the end is the whole of that day
	const auto days = Parse(R"({"rules": [{"type": "dates", "from": "2026-12-20", "to": "2027-01-02"}]})");
	EXPECT_FALSE(Active(days, Utc(2026, 12, 19, 23, 59)));
	EXPECT_TRUE(Active(days, Utc(2026, 12, 20)));
	EXPECT_TRUE(Active(days, Utc(2027, 1, 2, 23, 59)));
	EXPECT_FALSE(Active(days, Utc(2027, 1, 3)));
	EXPECT_FALSE(Active(days, Utc(2027, 12, 25))); // once only
	EXPECT_EQ(NextChange(days, Utc(2027, 1, 3)), std::nullopt);

	const auto times = Parse(R"({"rules": [{"type": "dates", "from": "2026-10-31T18:00", "to": "2026-11-01T02:00"}]})");
	EXPECT_FALSE(Active(times, Utc(2026, 10, 31, 17, 59)));
	EXPECT_TRUE(Active(times, Utc(2026, 10, 31, 18)));
	EXPECT_FALSE(Active(times, Utc(2026, 11, 1, 2)));
}

TEST(ScheduleRulesTests, WeekdaysAndTimesOfDay) {
	// 2026-09-25 is a Friday
	const auto weekend = Parse(R"({"rules": [{"type": "weekdays", "days": ["sat", "Sunday", 0]}]})");
	EXPECT_FALSE(Active(weekend, Utc(2026, 9, 25, 23, 59)));
	EXPECT_TRUE(Active(weekend, Utc(2026, 9, 26)));
	EXPECT_TRUE(Active(weekend, Utc(2026, 9, 27, 23, 59)));
	EXPECT_FALSE(Active(weekend, Utc(2026, 9, 28)));
	EXPECT_EQ(NextChange(weekend, Utc(2026, 9, 26, 10)), Utc(2026, 9, 28));

	// Past midnight
	const auto night = Parse(R"({"rules": [{"type": "time_of_day", "from": "22:00", "to": "02:00"}]})");
	EXPECT_TRUE(Active(night, Utc(2026, 9, 26, 23)));
	EXPECT_TRUE(Active(night, Utc(2026, 9, 27, 1, 59)));
	EXPECT_FALSE(Active(night, Utc(2026, 9, 27, 2)));
	EXPECT_FALSE(Active(night, Utc(2026, 9, 27, 21, 59)));
	EXPECT_EQ(NextChange(night, Utc(2026, 9, 27, 12)), Utc(2026, 9, 27, 22));

	// Friday evenings: every rule has to match
	const auto fridays = Parse(R"({"match": "all", "rules": [{"type": "weekdays", "days": ["fri"]}, {"type": "time_of_day", "from": "18:00", "to": "24:00"}]})");
	EXPECT_TRUE(Active(fridays, Utc(2026, 9, 25, 20)));
	EXPECT_FALSE(Active(fridays, Utc(2026, 9, 25, 17)));
	EXPECT_FALSE(Active(fridays, Utc(2026, 9, 26, 20)));
	EXPECT_EQ(NextChange(fridays, Utc(2026, 9, 26)), Utc(2026, 10, 2, 18));
}

TEST(ScheduleRulesTests, UtcOffsetMovesTheDays) {
	// October in UTC+2 starts at 22:00 UTC the day before
	const auto october = Parse(R"({"utcOffset": 120, "rules": [{"type": "yearly", "from": "10-01", "to": "10-31"}]})");
	EXPECT_FALSE(Active(october, Utc(2026, 9, 30, 21, 59)));
	EXPECT_TRUE(Active(october, Utc(2026, 9, 30, 22)));
	EXPECT_EQ(NextChange(october, Utc(2026, 9, 26)), Utc(2026, 9, 30, 22));
	const auto west = Parse(R"({"utcOffset": -300, "rules": [{"type": "time_of_day", "from": "18:00", "to": "20:00"}]})");
	EXPECT_TRUE(Active(west, Utc(2026, 9, 26, 23)));
}

TEST(ScheduleRulesTests, NotAndGroups) {
	// October but not on weekends (a group), or one other day
	const auto schedule = Parse(R"({"match": "any", "rules": [
		{"type": "group", "match": "all", "rules": [{"type": "yearly", "from": "10-01", "to": "10-31"}, {"type": "weekdays", "days": ["sat", "sun"], "not": true}]},
		{"type": "dates", "from": "2026-12-25", "to": "2026-12-25"}]})");
	EXPECT_TRUE(Active(schedule, Utc(2026, 10, 2, 12)));  // Friday
	EXPECT_FALSE(Active(schedule, Utc(2026, 10, 3, 12))); // Saturday
	EXPECT_TRUE(Active(schedule, Utc(2026, 12, 25, 12)));
	EXPECT_FALSE(Active(schedule, Utc(2026, 12, 26, 12)));
}

// Known full moons (UTC): 2024-10-17 11:26, 2025-10-07 03:48, 2026-10-26 04:12
TEST(ScheduleRulesTests, FullMoonTimes) {
	const std::vector<int64_t> known{ Utc(2024, 10, 17, 11, 26), Utc(2025, 10, 7, 3, 48), Utc(2026, 10, 26, 4, 12) };
	for (const auto time : known) {
		const auto found = Phases(eMoonPhase::FULL_MOON, time - 5 * 86400, time + 5 * 86400);
		ASSERT_EQ(found.size(), 1u);
		EXPECT_NEAR(static_cast<double>(found[0]), static_cast<double>(time), 5 * 60.0);
	}
	// One full moon a lunation: 12 or 13 a year
	const auto year = Phases(eMoonPhase::FULL_MOON, Utc(2026, 1, 1), Utc(2027, 1, 1));
	EXPECT_GE(year.size(), 12u);
	EXPECT_LE(year.size(), 13u);
	// New moon 2024-10-02 18:49 UTC
	const auto newMoon = Phases(eMoonPhase::NEW_MOON, Utc(2024, 9, 28), Utc(2024, 10, 6));
	ASSERT_EQ(newMoon.size(), 1u);
	EXPECT_NEAR(static_cast<double>(newMoon[0]), static_cast<double>(Utc(2024, 10, 2, 18, 49)), 5 * 60.0);
}

TEST(ScheduleRulesTests, FullMoonRules) {
	// The day it falls on
	const auto day = Parse(R"({"rules": [{"type": "moon", "phase": "full_moon"}]})");
	EXPECT_FALSE(Active(day, Utc(2025, 10, 6, 23, 59)));
	EXPECT_TRUE(Active(day, Utc(2025, 10, 7)));
	EXPECT_TRUE(Active(day, Utc(2025, 10, 7, 23, 59)));
	EXPECT_FALSE(Active(day, Utc(2025, 10, 8)));
	EXPECT_EQ(NextChange(day, Utc(2026, 10, 20)), Utc(2026, 10, 26));
	EXPECT_EQ(NextChange(day, Utc(2026, 10, 26)), Utc(2026, 10, 27));

	// The day in UTC-5 is the evening before in UTC: 2026-10-26 04:12 UTC is 23:12 on the 25th
	const auto west = Parse(R"({"utcOffset": -300, "rules": [{"type": "moon", "phase": "full_moon", "days": 0}]})");
	EXPECT_TRUE(Active(west, Utc(2026, 10, 25, 12)));
	EXPECT_FALSE(Active(west, Utc(2026, 10, 26, 12)));

	// A day either side
	const auto wide = Parse(R"({"rules": [{"type": "moon", "phase": "full_moon", "days": 1}]})");
	EXPECT_TRUE(Active(wide, Utc(2024, 10, 16)));
	EXPECT_TRUE(Active(wide, Utc(2024, 10, 18, 23)));
	EXPECT_FALSE(Active(wide, Utc(2024, 10, 19)));

	// Hours around the exact time
	const auto hours = Parse(R"({"rules": [{"type": "moon", "phase": "full_moon", "hours": 12}]})");
	EXPECT_TRUE(Active(hours, Utc(2024, 10, 17)));
	EXPECT_TRUE(Active(hours, Utc(2024, 10, 17, 23)));
	EXPECT_FALSE(Active(hours, Utc(2024, 10, 18)));
	EXPECT_FALSE(Active(hours, Utc(2024, 10, 16, 23)));

	// Full-moon nights: the day of the full moon, 20:00 to midnight
	const auto night = Parse(R"({"match": "all", "rules": [{"type": "moon", "phase": "full_moon"}, {"type": "time_of_day", "from": "20:00", "to": "24:00"}]})");
	EXPECT_EQ(NextChange(night, Utc(2026, 10, 1)), Utc(2026, 10, 26, 20));
	const auto windows = Windows(night, Utc(2026, 1, 1), Utc(2027, 1, 1));
	EXPECT_GE(windows.size(), 12u);
	for (const auto& window : windows) EXPECT_EQ(window.end - window.start, 4 * 3600);
}

TEST(ScheduleRulesTests, BadSchedules) {
	EXPECT_NE(ErrorOf(R"({"rules": []})").find("at least one rule"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "easter"}]})").find("Rule 1: type is one of"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "yearly", "from": "13-01", "to": "10-31"}]})").find("Rule 1"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "yearly", "from": "02-30", "to": "03-01"}]})").find("Rule 1"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "dates", "from": "2027-01-02", "to": "2026-12-20"}]})").find("after the start"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "time_of_day", "from": "18:00", "to": "18:00"}]})").find("same time"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "weekdays", "days": ["someday"]}]})").find("days are"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "moon", "phase": "blue_moon"}]})").find("phase is one of"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "moon", "phase": "full_moon", "days": 8}]})").find("days is"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"utcOffset": 900, "rules": [{"type": "weekdays", "days": ["mon"]}]})").find("utcOffset"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "group", "rules": [{"type": "group", "rules": [{"type": "group", "rules": [{"type": "group", "rules": [{"type": "weekdays", "days": ["mon"]}]}]}]}]}]})").find("deep"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "group", "match": "most", "rules": [{"type": "weekdays", "days": ["mon"]}]}]})").find("Rule 1: match"), std::string::npos);
	EXPECT_NE(ErrorOf(R"({"rules": [{"type": "group", "rules": [{"type": "weekdays", "days": ["mon"]}, {"type": "nope"}]}]})").find("Rule 1.2"), std::string::npos);
	EXPECT_NE(ErrorOf("not json").find("JSON"), std::string::npos);
}

TEST(ScheduleRulesTests, JsonRoundTrip) {
	const std::string text = R"({"utcOffset": 60, "match": "any", "rules": [
		{"type": "YEARLY", "from": "10-01", "to": "10-31"},
		{"type": "dates", "from": "2026-12-20T18:00", "to": "2027-01-02"},
		{"type": "weekdays", "days": ["fri", "sat"], "not": true},
		{"type": "time_of_day", "from": "18:00", "to": "02:00"},
		{"type": "moon", "phase": "Full_Moon", "hours": 6},
		{"type": "group", "match": "all", "rules": [{"type": "moon", "phase": "new_moon", "days": 1}]}]})";
	const auto first = ToJson(Parse(text));
	EXPECT_EQ(first["rules"][0]["type"], "yearly");
	EXPECT_EQ(first["rules"][1]["to"], "2027-01-02");
	EXPECT_EQ(first["rules"][4]["phase"], "full_moon");
	EXPECT_EQ(first["rules"][2]["not"], true);
	EXPECT_EQ(ToJson(Parse(first.dump())), first);
}

TEST(ScheduleRulesTests, Modes) {
	const std::string october = R"({"rules": [{"type": "yearly", "from": "10-01", "to": "10-31"}]})";
	EXPECT_TRUE(IsOn(eMode::SCHEDULED, october, Utc(2026, 10, 5)));
	EXPECT_FALSE(IsOn(eMode::SCHEDULED, october, Utc(2026, 9, 5)));
	EXPECT_TRUE(IsOn(eMode::ALWAYS_ON, october, Utc(2026, 9, 5)));
	EXPECT_FALSE(IsOn(eMode::OFF, october, Utc(2026, 10, 5)));
	EXPECT_FALSE(IsOn(eMode::SCHEDULED, "broken", Utc(2026, 10, 5)));
}

TEST(ScheduleRulesTests, OnceEvents) {
	const auto start = Utc(2026, 10, 1, 18), end = Utc(2026, 10, 2, 6);
	EXPECT_FALSE(IsOn(eMode::SCHEDULED, "", start, end, start - 1));
	EXPECT_TRUE(IsOn(eMode::SCHEDULED, "", start, end, start));
	EXPECT_FALSE(IsOn(eMode::SCHEDULED, "", start, end, end));
	EXPECT_TRUE(IsOn(eMode::ALWAYS_ON, "", start, end, end + 100));
	EXPECT_FALSE(IsOn(eMode::OFF, "", start, end, start));

	const auto windows = WindowsOf(eMode::SCHEDULED, "", start, end, Utc(2026, 10, 2), Utc(2026, 10, 3));
	ASSERT_EQ(windows.size(), 1u);
	EXPECT_EQ(windows[0].start, Utc(2026, 10, 2));
	EXPECT_EQ(windows[0].end, end);
	EXPECT_TRUE(WindowsOf(eMode::SCHEDULED, "", start, end, end, end + 86400).empty());
	EXPECT_TRUE(WindowsOf(eMode::OFF, "", start, end, start, end).empty());
	EXPECT_EQ(WindowsOf(eMode::ALWAYS_ON, "", 0, 0, start, end).size(), 1u);

	// Recurring: the schedule decides, startsAt and endsAt are ignored
	const std::string october = R"({"rules":[{"type":"yearly","from":"10-01","to":"10-31"}]})";
	EXPECT_TRUE(IsOn(eMode::SCHEDULED, october, 0, 0, Utc(2027, 10, 5)));
	EXPECT_EQ(WindowsOf(eMode::SCHEDULED, october, 0, 0, Utc(2026, 1, 1), Utc(2028, 1, 1)).size(), 2u);
}
