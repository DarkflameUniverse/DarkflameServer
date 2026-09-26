#pragma once

#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

/**
 * When a scheduled event is on (the dashboard's Events page: feature flags, vanity changes, live events, announcements
 * and restarts that switch on and off together). An event is off, on by its schedule, or always on. Its schedule is
 * either once (a start and an end) or recurring rules, as JSON: {"utcOffset": minutes, "match": "any"|"all", "rules": [rule, ...]},
 * each rule one of
 *   {"type": "yearly", "from": "10-01", "to": "10-31"}                  every year, whole days; may wrap the year end
 *   {"type": "dates", "from": "2026-12-20", "to": "2027-01-02"}          once; a date alone is the whole day,
 *                                                                        "2026-12-20T18:00" a time (the end is exclusive)
 *   {"type": "weekdays", "days": ["fri", "sat"]}
 *   {"type": "time_of_day", "from": "18:00", "to": "02:00"}             every day; may wrap midnight
 *   {"type": "moon", "phase": "full_moon", "days": 0}                   the day the phase falls on, +- days;
 *   {"type": "moon", "phase": "full_moon", "hours": 12}                 or +- hours around the exact time
 *   {"type": "group", "match": "all", "rules": [...]}                   rules inside rules
 * and any rule may have "not": true. Times are the server's UTC plus utcOffset (a fixed offset: no daylight saving).
 * Pure; unit tested.
 */
namespace ScheduleRules {
	// scheduled_events.mode; stored as numbers, so only append
	enum class eMode : uint8_t {
		OFF,       // never on
		SCHEDULED, // on while its schedule says so
		ALWAYS_ON  // on now, whatever the schedule says
	};

	enum class eRuleType : uint8_t { GROUP, YEARLY, DATES, WEEKDAYS, TIME_OF_DAY, MOON };
	enum class eMatch : uint8_t { ANY, ALL };
	enum class eMoonPhase : uint8_t { NEW_MOON, FIRST_QUARTER, FULL_MOON, LAST_QUARTER };

	constexpr size_t MAX_RULES = 64;
	constexpr size_t MAX_DEPTH = 4;
	constexpr int32_t MAX_UTC_OFFSET_MINUTES = 14 * 60;
	constexpr int32_t MAX_MOON_DAYS = 7;
	constexpr int32_t MAX_MOON_HOURS = 240;
	// How far ahead the next start or end is looked for
	constexpr int64_t HORIZON_SECONDS = 4 * 366 * 86400LL;
	constexpr double SYNODIC_MONTH_DAYS = 29.530588853;

	struct Rule {
		eRuleType type{ eRuleType::GROUP };
		bool invert{};                            // "not": on when the rule isn't
		eMatch match{ eMatch::ANY };              // GROUP
		std::vector<Rule> rules;                  // GROUP
		unsigned fromMonth{}, fromDay{}, toMonth{}, toDay{}; // YEARLY, both days included
		int64_t from{}, to{};                     // DATES: local seconds, [from, to)
		std::bitset<7> weekdays;                  // WEEKDAYS: 0 = Sunday
		unsigned fromMinute{}, toMinute{};        // TIME_OF_DAY: minutes of the day, [from, to); to < from wraps midnight
		eMoonPhase phase{ eMoonPhase::FULL_MOON }; // MOON
		int32_t days{};                           // MOON: the local day of the phase, and this many days either side
		std::optional<int32_t> hours;             // MOON: instead, this many hours either side of the exact time
	};

	struct Schedule {
		int32_t utcOffsetMinutes{};
		Rule root; // a GROUP
	};

	struct Window {
		int64_t start{};
		int64_t end{};
	};

	std::optional<Schedule> ParseSchedule(const nlohmann::json& json, std::string& error);
	// From the JSON text stored in scheduled_events.schedule
	std::optional<Schedule> ParseSchedule(const std::string& text, std::string& error);
	nlohmann::json ToJson(const Schedule& schedule);

	// Whether the schedule is on at `now` (unix seconds)
	bool Active(const Schedule& schedule, int64_t now);

	// The first time after `after` it turns on or off; nullopt if that isn't within `horizon` seconds
	std::optional<int64_t> NextChange(const Schedule& schedule, int64_t after, int64_t horizon = HORIZON_SECONDS);

	// The times it is on between from and to (windows are cut to fit), at most `max` of them
	std::vector<Window> Windows(const Schedule& schedule, int64_t from, int64_t to, size_t max = 100);

	// When a moon phase happens (unix seconds, to a minute or two; Meeus, Astronomical Algorithms ch. 49), for a
	// lunation counted from the new moon of 2000-01-06
	int64_t PhaseTime(int64_t lunation, eMoonPhase phase);

	// Every time a phase happens between from and to
	std::vector<int64_t> Phases(eMoonPhase phase, int64_t from, int64_t to);

	// Whether an event is on: never when OFF, always when ALWAYS_ON, when its schedule says for SCHEDULED
	bool IsOn(eMode mode, const std::string& schedule, int64_t now);

	/**
	 * The same for an event that is either recurring (schedule: rules as above) or once (schedule empty: on from startsAt
	 * until endsAt, unix seconds).
	 */
	bool IsOn(eMode mode, const std::string& schedule, int64_t startsAt, int64_t endsAt, int64_t now);

	// The times such an event is on between from and to, at most `max` of them (ALWAYS_ON: all of it; OFF or a bad schedule: none)
	std::vector<Window> WindowsOf(eMode mode, const std::string& schedule, int64_t startsAt, int64_t endsAt, int64_t from, int64_t to, size_t max = 100);

}
