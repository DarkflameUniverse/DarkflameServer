#include "ScheduleRules.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <tuple>

#include "magic_enum.hpp"
#include "CivilDate.h"

namespace {
	using namespace ScheduleRules;
	constexpr int64_t DAY = 86400;
	constexpr int64_t NEVER = std::numeric_limits<int64_t>::max();
	// Julian day of the unix epoch, and the new moon Meeus counts lunations from (2000-01-06)
	constexpr double UNIX_EPOCH_JD = 2440587.5;
	constexpr double LUNATION_ZERO_JDE = 2451550.09766;
	// Meeus gives the phases in terrestrial time, about this many seconds ahead of UTC these years
	constexpr double DELTA_T_SECONDS = 69.0;

	// "fri", "Friday" or 5
	std::optional<unsigned> WeekdayOf(const nlohmann::json& value) {
		static const char* NAMES[]{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
		if (value.is_number_integer()) {
			const auto day = value.get<int64_t>();
			return day >= 0 && day <= 7 ? std::optional<unsigned>(static_cast<unsigned>(day % 7)) : std::nullopt;
		}
		if (!value.is_string()) return std::nullopt;
		auto text = value.get<std::string>();
		if (text.size() < 3) return std::nullopt;
		for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		for (unsigned day = 0; day < 7; day++) {
			if (text.starts_with(NAMES[day])) return day;
		}
		return std::nullopt;
	}

	std::optional<unsigned> Digits(const std::string& text, size_t at, size_t count) {
		if (at + count > text.size()) return std::nullopt;
		unsigned value = 0;
		for (size_t i = at; i < at + count; i++) {
			if (!std::isdigit(static_cast<unsigned char>(text[i]))) return std::nullopt;
			value = value * 10 + static_cast<unsigned>(text[i] - '0');
		}
		return value;
	}

	// "10-31" -> (10, 31); Feb 29 is allowed (a year without one uses March 1)
	std::optional<std::pair<unsigned, unsigned>> MonthDay(const std::string& text) {
		if (text.size() != 5 || text[2] != '-') return std::nullopt;
		const auto month = Digits(text, 0, 2), day = Digits(text, 3, 2);
		if (!month || !day || *month < 1 || *month > 12 || *day < 1 || *day > CivilDate::DaysInMonth(2000, *month)) return std::nullopt;
		return std::pair{ *month, *day };
	}

	// "18:30" -> minutes of the day; "24:00" only where allowEnd
	std::optional<unsigned> TimeOfDay(const std::string& text, bool allowEnd) {
		if (text.size() != 5 || text[2] != ':') return std::nullopt;
		const auto hour = Digits(text, 0, 2), minute = Digits(text, 3, 2);
		if (!hour || !minute || *minute > 59) return std::nullopt;
		if (*hour == 24 && *minute == 0 && allowEnd) return 24 * 60;
		if (*hour > 23) return std::nullopt;
		return *hour * 60 + *minute;
	}

	// "2026-12-20" (dateOnly set) or "2026-12-20T18:00" -> local seconds
	std::optional<int64_t> DateTime(const std::string& text, bool& dateOnly) {
		if (text.size() < 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
		const auto year = Digits(text, 0, 4), month = Digits(text, 5, 2), day = Digits(text, 8, 2);
		if (!year || !month || !day || *year < 1970 || *month < 1 || *month > 12 || *day < 1 || *day > CivilDate::DaysInMonth(*year, *month)) return std::nullopt;
		int64_t seconds = CivilDate::DaysFromCivil(*year, *month, *day) * DAY;
		dateOnly = text.size() == 10;
		if (dateOnly) return seconds;
		if (text.size() != 16 || (text[10] != 'T' && text[10] != ' ')) return std::nullopt;
		const auto minutes = TimeOfDay(text.substr(11), false);
		if (!minutes) return std::nullopt;
		return seconds + static_cast<int64_t>(*minutes) * 60;
	}

	std::string Two(unsigned value) {
		return (value < 10 ? "0" : "") + std::to_string(value);
	}

	std::string DateText(int64_t localSeconds, bool withTime) {
		const auto day = CivilDate::DayOf(localSeconds);
		const auto date = CivilDate::CivilFromDays(day);
		std::string text = std::to_string(date.year) + "-" + Two(date.month) + "-" + Two(date.day);
		if (withTime) {
			const auto minutes = static_cast<unsigned>((localSeconds - day * DAY) / 60);
			text += "T" + Two(minutes / 60) + ":" + Two(minutes % 60);
		}
		return text;
	}

	std::string MinutesText(unsigned minutes) {
		return Two(minutes / 60) + ":" + Two(minutes % 60);
	}

	template<typename Enum>
	std::string Lower(Enum value) {
		std::string text(magic_enum::enum_name(value));
		for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return text;
	}

	template<typename Enum>
	std::string Choices() {
		std::string text;
		for (const auto value : magic_enum::enum_values<Enum>()) text += (text.empty() ? "" : ", ") + Lower(value);
		return text;
	}

	template<typename Enum>
	std::optional<Enum> EnumOf(const nlohmann::json& json, const char* key) {
		if (!json.contains(key) || !json[key].is_string()) return std::nullopt;
		return magic_enum::enum_cast<Enum>(json[key].get<std::string>(), magic_enum::case_insensitive);
	}

	std::string Text(const nlohmann::json& json, const char* key) {
		return json.contains(key) && json[key].is_string() ? json[key].get<std::string>() : "";
	}

	bool ParseRule(const nlohmann::json& json, Rule& rule, const std::string& where, size_t depth, size_t& count, std::string& error);

	bool ParseGroup(const nlohmann::json& json, Rule& rule, const std::string& where, size_t depth, size_t& count, std::string& error) {
		const auto prefix = where.empty() ? std::string() : where + ": ";
		if (depth > MAX_DEPTH) { error = prefix + "groups go at most " + std::to_string(MAX_DEPTH) + " deep"; return false; }
		rule.type = eRuleType::GROUP;
		rule.match = eMatch::ANY;
		if (json.contains("match")) {
			const auto match = EnumOf<eMatch>(json, "match");
			if (!match) { error = prefix + "match is one of " + Choices<eMatch>(); return false; }
			rule.match = *match;
		}
		if (!json.contains("rules") || !json["rules"].is_array() || json["rules"].empty()) { error = prefix + "add at least one rule"; return false; }
		for (size_t i = 0; i < json["rules"].size(); i++) {
			Rule child;
			if (!ParseRule(json["rules"][i], child, (where.empty() ? "Rule " : where + ".") + std::to_string(i + 1), depth + 1, count, error)) return false;
			rule.rules.push_back(std::move(child));
		}
		return true;
	}

	bool ParseRule(const nlohmann::json& json, Rule& rule, const std::string& where, size_t depth, size_t& count, std::string& error) {
		const auto fail = [&](const std::string& message) { error = where + ": " + message; return false; };
		if (++count > MAX_RULES) return fail("a schedule has at most " + std::to_string(MAX_RULES) + " rules");
		if (!json.is_object()) return fail("each rule is an object");
		const auto type = EnumOf<eRuleType>(json, "type");
		if (!type) return fail("type is one of " + Choices<eRuleType>());
		rule.type = *type;
		if (json.contains("not") && !json["not"].is_boolean()) return fail("not is true or false");
		rule.invert = json.contains("not") && json["not"].get<bool>();
		switch (rule.type) {
		case eRuleType::GROUP:
			return ParseGroup(json, rule, where, depth, count, error);
		case eRuleType::YEARLY: {
			const auto from = MonthDay(Text(json, "from")), to = MonthDay(Text(json, "to"));
			if (!from || !to) return fail("from and to are a month and day, like 10-01");
			std::tie(rule.fromMonth, rule.fromDay) = *from;
			std::tie(rule.toMonth, rule.toDay) = *to;
			return true;
		}
		case eRuleType::DATES: {
			bool fromDate = false, toDate = false;
			const auto from = DateTime(Text(json, "from"), fromDate), to = DateTime(Text(json, "to"), toDate);
			if (!from || !to) return fail("from and to are dates like 2026-12-20, or with a time like 2026-12-20T18:00");
			rule.from = *from;
			// A date alone as the end means the whole of that day
			rule.to = *to + (toDate ? DAY : 0);
			if (rule.to <= rule.from) return fail("the end has to be after the start");
			return true;
		}
		case eRuleType::WEEKDAYS: {
			if (!json.contains("days") || !json["days"].is_array() || json["days"].empty()) return fail("pick at least one day");
			for (const auto& day : json["days"]) {
				const auto weekday = WeekdayOf(day);
				if (!weekday) return fail("days are sun, mon, tue, wed, thu, fri or sat");
				rule.weekdays.set(*weekday);
			}
			return true;
		}
		case eRuleType::TIME_OF_DAY: {
			const auto from = TimeOfDay(Text(json, "from"), false), to = TimeOfDay(Text(json, "to"), true);
			if (!from || !to) return fail("from and to are times like 18:00 (to may be 24:00)");
			if (*from == *to % (24 * 60)) return fail("from and to are the same time");
			rule.fromMinute = *from;
			rule.toMinute = *to;
			return true;
		}
		case eRuleType::MOON: {
			const auto phase = EnumOf<eMoonPhase>(json, "phase");
			if (!phase) return fail("phase is one of " + Choices<eMoonPhase>());
			rule.phase = *phase;
			if (json.contains("hours")) {
				if (!json["hours"].is_number_integer() || json["hours"].get<int64_t>() < 1 || json["hours"].get<int64_t>() > MAX_MOON_HOURS) {
					return fail("hours is 1 to " + std::to_string(MAX_MOON_HOURS));
				}
				rule.hours = json["hours"].get<int32_t>();
			} else if (json.contains("days")) {
				if (!json["days"].is_number_integer() || json["days"].get<int64_t>() < 0 || json["days"].get<int64_t>() > MAX_MOON_DAYS) {
					return fail("days is 0 to " + std::to_string(MAX_MOON_DAYS));
				}
				rule.days = json["days"].get<int32_t>();
			}
			return true;
		}
		}
		return fail("unknown type");
	}

	nlohmann::json RuleJson(const Rule& rule) {
		nlohmann::json json{ {"type", Lower(rule.type)} };
		switch (rule.type) {
		case eRuleType::GROUP: {
			json["match"] = Lower(rule.match);
			json["rules"] = nlohmann::json::array();
			for (const auto& child : rule.rules) json["rules"].push_back(RuleJson(child));
			break;
		}
		case eRuleType::YEARLY:
			json["from"] = Two(rule.fromMonth) + "-" + Two(rule.fromDay);
			json["to"] = Two(rule.toMonth) + "-" + Two(rule.toDay);
			break;
		case eRuleType::DATES: {
			json["from"] = DateText(rule.from, rule.from % DAY != 0);
			// A whole-day end goes back as the last day
			json["to"] = rule.to % DAY == 0 ? DateText(rule.to - DAY, false) : DateText(rule.to, true);
			break;
		}
		case eRuleType::WEEKDAYS: {
			static const char* NAMES[]{ "sun", "mon", "tue", "wed", "thu", "fri", "sat" };
			json["days"] = nlohmann::json::array();
			for (unsigned day = 0; day < 7; day++) if (rule.weekdays[day]) json["days"].push_back(NAMES[day]);
			break;
		}
		case eRuleType::TIME_OF_DAY:
			json["from"] = MinutesText(rule.fromMinute);
			json["to"] = MinutesText(rule.toMinute);
			break;
		case eRuleType::MOON:
			json["phase"] = Lower(rule.phase);
			if (rule.hours) json["hours"] = *rule.hours;
			else json["days"] = rule.days;
			break;
		}
		if (rule.invert) json["not"] = true;
		return json;
	}

	double PhaseFraction(eMoonPhase phase) {
		return static_cast<double>(static_cast<uint8_t>(phase)) * 0.25;
	}

	// The lunation (counted as PhaseTime does) around a time
	int64_t LunationAt(int64_t time) {
		const double jd = static_cast<double>(time) / DAY + UNIX_EPOCH_JD;
		return static_cast<int64_t>(std::floor((jd - LUNATION_ZERO_JDE) / SYNODIC_MONTH_DAYS));
	}

	// The unix times a moon rule is on around the phase of one lunation
	Window MoonWindow(const Rule& rule, int64_t lunation, int32_t offsetSeconds) {
		const auto instant = PhaseTime(lunation, rule.phase);
		if (rule.hours) return { instant - *rule.hours * 3600LL, instant + *rule.hours * 3600LL };
		const auto day = CivilDate::DayOf(instant + offsetSeconds);
		return { (day - rule.days) * DAY - offsetSeconds, (day + rule.days + 1) * DAY - offsetSeconds };
	}

	bool Matches(const Rule& rule, int64_t now, int32_t offsetSeconds) {
		const int64_t local = now + offsetSeconds;
		const auto day = CivilDate::DayOf(local);
		bool on = false;
		switch (rule.type) {
		case eRuleType::GROUP:
			if (rule.match == eMatch::ALL) on = std::all_of(rule.rules.begin(), rule.rules.end(), [&](const Rule& r) { return Matches(r, now, offsetSeconds); });
			else on = std::any_of(rule.rules.begin(), rule.rules.end(), [&](const Rule& r) { return Matches(r, now, offsetSeconds); });
			break;
		case eRuleType::YEARLY: {
			const auto date = CivilDate::CivilFromDays(day);
			const auto today = date.month * 100 + date.day, from = rule.fromMonth * 100 + rule.fromDay, to = rule.toMonth * 100 + rule.toDay;
			on = from <= to ? today >= from && today <= to : today >= from || today <= to;
			break;
		}
		case eRuleType::DATES:
			on = local >= rule.from && local < rule.to;
			break;
		case eRuleType::WEEKDAYS:
			on = rule.weekdays[CivilDate::Weekday(day)];
			break;
		case eRuleType::TIME_OF_DAY: {
			const auto minute = static_cast<unsigned>((local - day * DAY) / 60);
			on = rule.fromMinute < rule.toMinute ? minute >= rule.fromMinute && minute < rule.toMinute : minute >= rule.fromMinute || minute < rule.toMinute;
			break;
		}
		case eRuleType::MOON: {
			const auto lunation = LunationAt(now);
			for (auto k = lunation - 2; k <= lunation + 2 && !on; k++) {
				const auto window = MoonWindow(rule, k, offsetSeconds);
				on = now >= window.start && now < window.end;
			}
			break;
		}
		}
		return on != rule.invert;
	}

	// The first time after `now` a rule could turn on or off (its state can't change before it)
	int64_t NextBoundary(const Rule& rule, int64_t now, int32_t offsetSeconds) {
		const int64_t local = now + offsetSeconds;
		const auto day = CivilDate::DayOf(local);
		int64_t next = NEVER;
		const auto consider = [&](int64_t time) { if (time > now) next = std::min(next, time); };
		switch (rule.type) {
		case eRuleType::GROUP:
			for (const auto& child : rule.rules) next = std::min(next, NextBoundary(child, now, offsetSeconds));
			break;
		case eRuleType::YEARLY:
		case eRuleType::WEEKDAYS:
			// Whole days: the next midnight
			consider((day + 1) * DAY - offsetSeconds);
			break;
		case eRuleType::DATES:
			consider(rule.from - offsetSeconds);
			consider(rule.to - offsetSeconds);
			break;
		case eRuleType::TIME_OF_DAY:
			for (auto d = day; d <= day + 1; d++) {
				consider(d * DAY + rule.fromMinute * 60LL - offsetSeconds);
				consider(d * DAY + rule.toMinute * 60LL - offsetSeconds);
			}
			break;
		case eRuleType::MOON: {
			const auto lunation = LunationAt(now);
			for (auto k = lunation - 2; k <= lunation + 3; k++) {
				const auto window = MoonWindow(rule, k, offsetSeconds);
				consider(window.start);
				consider(window.end);
			}
			break;
		}
		}
		return next;
	}
}

std::optional<Schedule> ScheduleRules::ParseSchedule(const nlohmann::json& json, std::string& error) {
	if (!json.is_object()) { error = "The schedule is an object: {utcOffset, match, rules}"; return std::nullopt; }
	Schedule schedule;
	if (json.contains("utcOffset")) {
		if (!json["utcOffset"].is_number_integer() || std::abs(json["utcOffset"].get<int64_t>()) > MAX_UTC_OFFSET_MINUTES) {
			error = "utcOffset is minutes from UTC, -840 to 840";
			return std::nullopt;
		}
		schedule.utcOffsetMinutes = json["utcOffset"].get<int32_t>();
	}
	size_t count = 0;
	if (!ParseGroup(json, schedule.root, "", 1, count, error)) return std::nullopt;
	return schedule;
}

std::optional<Schedule> ScheduleRules::ParseSchedule(const std::string& text, std::string& error) {
	const auto json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded()) { error = "The schedule isn't valid JSON"; return std::nullopt; }
	return ParseSchedule(json, error);
}

nlohmann::json ScheduleRules::ToJson(const Schedule& schedule) {
	auto json = RuleJson(schedule.root);
	json.erase("type");
	json.erase("not");
	json["utcOffset"] = schedule.utcOffsetMinutes;
	return json;
}

bool ScheduleRules::Active(const Schedule& schedule, int64_t now) {
	return Matches(schedule.root, now, schedule.utcOffsetMinutes * 60);
}

std::optional<int64_t> ScheduleRules::NextChange(const Schedule& schedule, int64_t after, int64_t horizon) {
	const int32_t offset = schedule.utcOffsetMinutes * 60;
	const bool on = Matches(schedule.root, after, offset);
	const auto limit = after + horizon;
	// Between two boundaries nothing changes, so only they need checking (about two a day at most for most schedules)
	for (int64_t time = after; time < limit;) {
		time = NextBoundary(schedule.root, time, offset);
		if (time == NEVER || time > limit) break;
		if (Matches(schedule.root, time, offset) != on) return time;
	}
	return std::nullopt;
}

std::vector<Window> ScheduleRules::Windows(const Schedule& schedule, int64_t from, int64_t to, size_t max) {
	std::vector<Window> windows;
	if (to <= from) return windows;
	bool on = Active(schedule, from);
	int64_t start = from;
	for (int64_t time = from; windows.size() < max;) {
		const auto next = NextChange(schedule, time, to - time);
		if (!next) {
			if (on) windows.push_back({ start, to });
			break;
		}
		if (on) windows.push_back({ start, *next });
		else start = *next;
		on = !on;
		time = *next;
	}
	return windows;
}

int64_t ScheduleRules::PhaseTime(int64_t lunation, eMoonPhase phase) {
	const double q = PhaseFraction(phase);
	const double k = static_cast<double>(lunation) + q;
	const double T = k / 1236.85;
	const double jdeMean = LUNATION_ZERO_JDE + 29.530588861 * k + 0.00015437 * T * T - 0.000000150 * T * T * T + 0.00000000073 * T * T * T * T;
	const auto rad = [](double degrees) { return std::fmod(degrees, 360.0) * 3.14159265358979323846 / 180.0; };
	const double E = 1 - 0.002516 * T - 0.0000074 * T * T;
	const double M = rad(2.5534 + 29.10535670 * k - 0.0000014 * T * T - 0.00000011 * T * T * T);
	const double Mp = rad(201.5643 + 385.81693528 * k + 0.0107582 * T * T + 0.00001238 * T * T * T - 0.000000058 * T * T * T * T);
	const double F = rad(160.7108 + 390.67050284 * k - 0.0016118 * T * T - 0.00000227 * T * T * T + 0.000000011 * T * T * T * T);
	const double O = rad(124.7746 - 1.56375588 * k + 0.0020672 * T * T + 0.00000215 * T * T * T);
	const auto s = [](double x) { return std::sin(x); };

	double correction = 0;
	if (phase == eMoonPhase::NEW_MOON || phase == eMoonPhase::FULL_MOON) {
		const bool isNew = phase == eMoonPhase::NEW_MOON;
		correction = (isNew ? -0.40720 : -0.40614) * s(Mp) + (isNew ? 0.17241 : 0.17302) * E * s(M) + (isNew ? 0.01608 : 0.01614) * s(2 * Mp)
			+ (isNew ? 0.01039 : 0.01043) * s(2 * F) + (isNew ? 0.00739 : 0.00734) * E * s(Mp - M) + (isNew ? -0.00514 : -0.00515) * E * s(Mp + M)
			+ (isNew ? 0.00208 : 0.00209) * E * E * s(2 * M) - 0.00111 * s(Mp - 2 * F) - 0.00057 * s(Mp + 2 * F) + 0.00056 * E * s(2 * Mp + M)
			- 0.00042 * s(3 * Mp) + 0.00042 * E * s(M + 2 * F) + 0.00038 * E * s(M - 2 * F) - 0.00024 * E * s(2 * Mp - M) - 0.00017 * s(O)
			- 0.00007 * s(Mp + 2 * M) + 0.00004 * s(2 * Mp - 2 * F) + 0.00004 * s(3 * M) + 0.00003 * s(Mp + M - 2 * F) + 0.00003 * s(2 * Mp + 2 * F)
			- 0.00003 * s(Mp + M + 2 * F) + 0.00003 * s(Mp - M + 2 * F) - 0.00002 * s(Mp - M - 2 * F) - 0.00002 * s(3 * Mp + M) + 0.00002 * s(4 * Mp);
	} else {
		correction = -0.62801 * s(Mp) + 0.17172 * E * s(M) - 0.01183 * E * s(Mp + M) + 0.00862 * s(2 * Mp) + 0.00804 * s(2 * F)
			+ 0.00454 * E * s(Mp - M) + 0.00204 * E * E * s(2 * M) - 0.00180 * s(Mp - 2 * F) - 0.00070 * s(Mp + 2 * F) - 0.00040 * s(3 * Mp)
			- 0.00034 * E * s(2 * Mp - M) + 0.00032 * E * s(M + 2 * F) + 0.00032 * E * s(M - 2 * F) - 0.00028 * E * E * s(Mp + 2 * M)
			+ 0.00027 * E * s(2 * Mp + M) - 0.00017 * s(O) - 0.00005 * s(Mp - M - 2 * F) + 0.00004 * s(2 * Mp + 2 * F) - 0.00004 * s(Mp + M + 2 * F)
			+ 0.00004 * s(Mp - 2 * M) + 0.00003 * s(Mp + M - 2 * F) + 0.00003 * s(3 * M) + 0.00002 * s(2 * Mp - 2 * F) + 0.00002 * s(Mp - M + 2 * F)
			- 0.00002 * s(3 * Mp + M);
		const double W = 0.00306 - 0.00038 * E * std::cos(M) + 0.00026 * std::cos(Mp) - 0.00002 * std::cos(Mp - M) + 0.00002 * std::cos(Mp + M) + 0.00002 * std::cos(2 * F);
		correction += phase == eMoonPhase::FIRST_QUARTER ? W : -W;
	}

	// The planetary terms, the same for every phase
	const double A[]{ 299.77 + 0.107408 * k - 0.009173 * T * T, 251.88 + 0.016321 * k, 251.83 + 26.651886 * k, 349.42 + 36.412478 * k,
		84.66 + 18.206239 * k, 141.74 + 53.303771 * k, 207.14 + 2.453732 * k, 154.84 + 7.306860 * k, 34.52 + 27.261239 * k,
		207.19 + 0.121824 * k, 291.34 + 1.844379 * k, 161.72 + 24.198154 * k, 239.56 + 25.513099 * k, 331.55 + 3.592518 * k };
	const double C[]{ 0.000325, 0.000165, 0.000164, 0.000126, 0.000110, 0.000062, 0.000060, 0.000056, 0.000047, 0.000042, 0.000040,
		0.000037, 0.000035, 0.000023 };
	double planetary = 0;
	for (size_t i = 0; i < std::size(A); i++) planetary += C[i] * s(rad(A[i]));

	const double jde = jdeMean + correction + planetary;
	return static_cast<int64_t>(std::llround((jde - UNIX_EPOCH_JD) * DAY - DELTA_T_SECONDS));
}

std::vector<int64_t> ScheduleRules::Phases(eMoonPhase phase, int64_t from, int64_t to) {
	std::vector<int64_t> times;
	for (auto k = LunationAt(from) - 1; k <= LunationAt(to) + 1; k++) {
		const auto time = PhaseTime(k, phase);
		if (time >= from && time < to) times.push_back(time);
	}
	return times;
}

bool ScheduleRules::IsOn(eMode mode, const std::string& schedule, int64_t now) {
	if (mode == eMode::ALWAYS_ON) return true;
	if (mode != eMode::SCHEDULED) return false;
	std::string error;
	const auto parsed = ParseSchedule(schedule, error);
	return parsed && Active(*parsed, now);
}

bool ScheduleRules::IsOn(eMode mode, const std::string& schedule, int64_t startsAt, int64_t endsAt, int64_t now) {
	if (mode == eMode::SCHEDULED && schedule.empty()) return now >= startsAt && now < endsAt;
	return IsOn(mode, schedule, now);
}

std::vector<Window> ScheduleRules::WindowsOf(eMode mode, const std::string& schedule, int64_t startsAt, int64_t endsAt, int64_t from, int64_t to, size_t max) {
	if (to <= from || max == 0 || mode == eMode::OFF) return {};
	if (mode == eMode::ALWAYS_ON) return { { from, to } };
	if (schedule.empty()) {
		if (endsAt <= from || startsAt >= to) return {};
		return { { std::max(startsAt, from), std::min(endsAt, to) } };
	}
	std::string error;
	const auto parsed = ParseSchedule(schedule, error);
	return parsed ? Windows(*parsed, from, to, max) : std::vector<Window>{};
}
