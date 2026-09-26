#pragma once

#include <cstdint>

/**
 * Calendar dates as days since 1970-01-01 and back (Howard Hinnant's algorithms), for schedules that work in whole
 * days: the dashboard's cron schedules (Cron.h) and the vanity event rules (VanityEvents.h).
 */
namespace CivilDate {
	struct Date { int64_t year; unsigned month; unsigned day; };

	inline int64_t DaysFromCivil(int64_t y, unsigned m, unsigned d) {
		y -= m <= 2;
		const int64_t era = (y >= 0 ? y : y - 399) / 400;
		const auto yoe = static_cast<unsigned>(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + static_cast<int64_t>(doe) - 719468;
	}

	inline Date CivilFromDays(int64_t z) {
		z += 719468;
		const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
		const auto doe = static_cast<unsigned>(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned mp = (5 * doy + 2) / 153;
		const unsigned d = doy - (153 * mp + 2) / 5 + 1;
		const unsigned m = mp < 10 ? mp + 3 : mp - 9;
		return { static_cast<int64_t>(yoe) + era * 400 + (m <= 2), m, d };
	}

	// 0 = Sunday; 1970-01-01 was a Thursday
	inline unsigned Weekday(int64_t daysSinceEpoch) {
		return static_cast<unsigned>(((daysSinceEpoch % 7) + 11) % 7);
	}

	// The day a unix time falls on, rounding down for times before 1970 too
	inline int64_t DayOf(int64_t seconds) {
		return seconds >= 0 ? seconds / 86400 : (seconds - 86399) / 86400;
	}

	inline bool IsLeapYear(int64_t year) {
		return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
	}

	inline unsigned DaysInMonth(int64_t year, unsigned month) {
		static constexpr unsigned DAYS[]{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		return month == 2 && IsLeapYear(year) ? 29 : DAYS[(month - 1) % 12];
	}
}
