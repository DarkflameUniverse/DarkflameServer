#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "CivilDate.h"
#include "GeneralUtils.h"

/**
 * Schedules for dashboard tasks, in UTC. Accepts:
 *  - standard 5-field cron: "minute hour day-of-month month day-of-week", each field *, a number, a range (1-5),
 *    a step (*\/15, 0-30/10) or a comma list; months and weekdays also by name (jan, mon); Sunday is 0 or 7.
 *    As in cron, when both day fields are restricted a day matching either one runs.
 *  - @hourly, @daily (@midnight), @weekly, @monthly, @yearly (@annually)
 *  - "@every 30s", "@every 10m", "@every 2h", "@every 1d": a fixed interval (at least 10 seconds)
 * Pure; unit tested.
 */
namespace Cron {
	constexpr int64_t MIN_INTERVAL_SECONDS = 10;

	struct Schedule {
		int64_t intervalSeconds{}; // "@every"; 0 for cron fields
		std::bitset<60> minutes;
		std::bitset<24> hours;
		std::bitset<32> days;   // 1-31
		std::bitset<13> months; // 1-12
		std::bitset<7> weekdays; // 0 = Sunday
		bool anyDay{};
		bool anyWeekday{};
	};

	namespace Detail {
		// Days since 1970-01-01 for a UTC date, and back (shared with the vanity event rules)
		using CivilDate::DaysFromCivil;
		using CivilDate::CivilFromDays;
		using CivilDate::Date;

		inline std::string Lower(std::string_view text) {
			std::string out(text);
			for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return out;
		}

		inline std::optional<int> Value(std::string_view text, const std::vector<std::string_view>& names, int nameBase) {
			const auto lower = Lower(text);
			for (size_t i = 0; i < names.size(); i++) {
				if (lower == names[i]) return static_cast<int>(i) + nameBase;
			}
			if (text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos || text.size() > 4) return std::nullopt;
			return GeneralUtils::TryParse<int>(std::string(text));
		}

		// Parse one field into the set of allowed values; error names what is wrong
		template<size_t N>
		bool Field(std::string_view text, int low, int high, const std::vector<std::string_view>& names, int nameBase,
			std::bitset<N>& out, bool& any, std::string& error, const char* label) {
			any = text == "*";
			size_t start = 0;
			while (start <= text.size()) {
				const auto comma = std::min(text.find(',', start), text.size());
				const auto part = text.substr(start, comma - start);
				start = comma + 1;
				if (part.empty()) { error = std::string("Empty value in the ") + label + " field"; return false; }

				auto range = part;
				int step = 1;
				if (const auto slash = part.find('/'); slash != std::string_view::npos) {
					range = part.substr(0, slash);
					const auto parsed = Value(part.substr(slash + 1), {}, 0);
					if (!parsed || *parsed <= 0) { error = std::string("Bad step in the ") + label + " field: " + std::string(part); return false; }
					step = *parsed;
				}
				int from = low, to = high;
				if (range != "*") {
					const auto dash = range.find('-');
					const auto first = Value(range.substr(0, dash), names, nameBase);
					if (!first) { error = std::string("Bad value in the ") + label + " field: " + std::string(part); return false; }
					from = *first;
					to = *first;
					if (dash != std::string_view::npos) {
						const auto last = Value(range.substr(dash + 1), names, nameBase);
						if (!last) { error = std::string("Bad range in the ") + label + " field: " + std::string(part); return false; }
						to = *last;
					} else if (step != 1) {
						to = high; // "5/15" means from 5 to the end, every 15
					}
				}
				if (from < low || to > high || from > to) {
					error = std::string("Out of range in the ") + label + " field (" + std::to_string(low) + "-" + std::to_string(high) + "): " + std::string(part);
					return false;
				}
				for (int value = from; value <= to; value += step) out.set(static_cast<size_t>(value));
				if (comma == text.size()) break;
			}
			return true;
		}
	}

	inline std::optional<Schedule> Parse(const std::string& input, std::string& error) {
		// Split on whitespace
		std::vector<std::string> fields;
		std::string current;
		for (const char c : input) {
			if (std::isspace(static_cast<unsigned char>(c))) {
				if (!current.empty()) fields.push_back(std::move(current));
				current.clear();
			} else {
				current += c;
			}
		}
		if (!current.empty()) fields.push_back(std::move(current));
		if (fields.empty()) { error = "The schedule is empty"; return std::nullopt; }

		const auto keyword = Detail::Lower(fields[0]);
		if (keyword == "@every") {
			if (fields.size() != 2 || fields[1].size() < 2) { error = "Use @every followed by a number and s, m, h or d, like @every 15m"; return std::nullopt; }
			const auto unit = static_cast<char>(std::tolower(static_cast<unsigned char>(fields[1].back())));
			const auto amount = Detail::Value(std::string_view(fields[1]).substr(0, fields[1].size() - 1), {}, 0);
			const int64_t multiplier = unit == 's' ? 1 : unit == 'm' ? 60 : unit == 'h' ? 3600 : unit == 'd' ? 86400 : 0;
			if (!amount || *amount <= 0 || multiplier == 0) { error = "Use @every followed by a number and s, m, h or d, like @every 15m"; return std::nullopt; }
			Schedule schedule;
			schedule.intervalSeconds = static_cast<int64_t>(*amount) * multiplier;
			if (schedule.intervalSeconds < MIN_INTERVAL_SECONDS) { error = "The shortest interval is 10 seconds"; return std::nullopt; }
			return schedule;
		}

		if (fields.size() == 1 && keyword.starts_with('@')) {
			static const std::array<std::pair<std::string_view, std::string_view>, 7> aliases{ {
				{"@hourly", "0 * * * *"}, {"@daily", "0 0 * * *"}, {"@midnight", "0 0 * * *"}, {"@weekly", "0 0 * * 0"},
				{"@monthly", "0 0 1 * *"}, {"@yearly", "0 0 1 1 *"}, {"@annually", "0 0 1 1 *"} } };
			for (const auto& [name, expansion] : aliases) {
				if (keyword == name) return Parse(std::string(expansion), error);
			}
			error = "Unknown schedule " + fields[0];
			return std::nullopt;
		}

		if (fields.size() != 5) { error = "A cron schedule has 5 fields: minute hour day-of-month month day-of-week"; return std::nullopt; }
		static const std::vector<std::string_view> MONTHS{ "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
		static const std::vector<std::string_view> WEEKDAYS{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
		Schedule schedule;
		bool ignored = false;
		std::bitset<8> weekdays;
		if (!Detail::Field(fields[0], 0, 59, {}, 0, schedule.minutes, ignored, error, "minute")) return std::nullopt;
		if (!Detail::Field(fields[1], 0, 23, {}, 0, schedule.hours, ignored, error, "hour")) return std::nullopt;
		if (!Detail::Field(fields[2], 1, 31, {}, 0, schedule.days, schedule.anyDay, error, "day-of-month")) return std::nullopt;
		if (!Detail::Field(fields[3], 1, 12, MONTHS, 1, schedule.months, ignored, error, "month")) return std::nullopt;
		if (!Detail::Field(fields[4], 0, 7, WEEKDAYS, 0, weekdays, schedule.anyWeekday, error, "day-of-week")) return std::nullopt;
		for (size_t day = 0; day < 7; day++) schedule.weekdays[day] = weekdays[day];
		if (weekdays[7]) schedule.weekdays.set(0);
		return schedule;
	}

	inline std::optional<Schedule> Parse(const std::string& input) {
		std::string error;
		return Parse(input, error);
	}

	inline bool DayMatches(const Schedule& schedule, int64_t daysSinceEpoch, unsigned dayOfMonth) {
		const auto weekday = static_cast<size_t>(CivilDate::Weekday(daysSinceEpoch));
		const bool dayOk = schedule.days[dayOfMonth];
		const bool weekdayOk = schedule.weekdays[weekday];
		if (schedule.anyDay && schedule.anyWeekday) return true;
		if (schedule.anyDay) return weekdayOk;
		if (schedule.anyWeekday) return dayOk;
		return dayOk || weekdayOk;
	}

	// The first time strictly after `after` (unix seconds) the schedule fires; nullopt if never (like "0 0 31 2 *")
	inline std::optional<int64_t> Next(const Schedule& schedule, int64_t after) {
		if (schedule.intervalSeconds > 0) return after + schedule.intervalSeconds;

		constexpr int64_t DAY = 86400;
		int64_t minuteStart = (after >= 0 ? after / 60 : (after - 59) / 60) * 60 + 60;
		int64_t day = minuteStart >= 0 ? minuteStart / DAY : (minuteStart - DAY + 1) / DAY;
		int64_t secondOfDay = minuteStart - day * DAY;
		// Five years covers every valid combination (Feb 29 on a given weekday included)
		for (int64_t tries = 0; tries < 366 * 5; tries++, day++, secondOfDay = 0) {
			const auto date = Detail::CivilFromDays(day);
			if (!schedule.months[date.month]) {
				// Jump to the first of next month
				const auto nextMonth = date.month == 12 ? Detail::DaysFromCivil(date.year + 1, 1, 1) : Detail::DaysFromCivil(date.year, date.month + 1, 1);
				tries += nextMonth - day - 1;
				day = nextMonth - 1;
				continue;
			}
			if (!DayMatches(schedule, day, date.day)) continue;
			for (auto hour = static_cast<size_t>(secondOfDay / 3600); hour < 24; hour++) {
				if (!schedule.hours[hour]) continue;
				const auto firstMinute = hour == static_cast<size_t>(secondOfDay / 3600) ? static_cast<size_t>((secondOfDay % 3600) / 60) : 0;
				for (auto minute = firstMinute; minute < 60; minute++) {
					if (schedule.minutes[minute]) return day * DAY + static_cast<int64_t>(hour) * 3600 + static_cast<int64_t>(minute) * 60;
				}
			}
		}
		return std::nullopt;
	}
}
