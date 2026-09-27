#ifndef __PROPERTYREPUTATIONRULES__H__
#define __PROPERTYREPUTATIONRULES__H__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>

/**
 * Property reputation (issues #636 and #637), the rules without the game around them so they can be unit tested.
 *
 * Early live gave a property 1 point per minute a visitor spent on it (PropertyTemplate.reputationPerMinute), which
 * was easy to farm, and later changed to an unpublished algorithm to stop that. This one keeps "time spent by other
 * people" as the signal but makes every way of inflating it expensive:
 *
 * - Only other people count: not the owner's account (so none of the owner's characters), not accounts linked to it
 *   (same play key, email or login address; property_reputation_ignore_linked) and not staff
 *   (property_reputation_ignore_staff).
 * - A visit earns nothing for its first property_reputation_min_visit seconds (WorldConfig's propertyReputationDelay,
 *   120), so hopping in and out does nothing.
 * - After that each minute the visitor was active (moved at least MIN_MOVE units since the last minute;
 *   property_reputation_require_activity) earns reputationPerMinute x property_reputation_multiplier, for at most
 *   property_reputation_max_minutes minutes per visit, so parking an idle alt on a property earns nothing.
 * - Repeat visitors count less: a visitor who already gave the property reputation on d of the last
 *   property_reputation_repeat_days days earns 1 / (1 + property_reputation_repeat_falloff x d) as much. Many different
 *   visitors are worth more than one visitor coming back every day.
 * - Caps per UTC day: one visitor account gives a property at most property_reputation_visitor_daily_cap points, and a
 *   property gets at most property_reputation_daily_cap from everyone together.
 *
 * Fractions carry over between minutes of a visit; points over a cap are dropped, not carried.
 */
namespace PropertyReputationRules {
	constexpr float MIN_MOVE = 2.0f; // about one minifigure width

	struct Params {
		int64_t minVisitSeconds{ 120 };
		double multiplier{ 1.0 };
		int64_t maxMinutesPerVisit{ 30 };
		int64_t visitorDailyCap{ 30 };
		int64_t propertyDailyCap{ 300 };
		double repeatFalloff{ 0.5 };
		bool requireActivity{ true };
		bool ignoreStaff{ true };
		bool ignoreLinked{ true };
	};

	struct Position {
		float x{}, y{}, z{};
	};

	// Whether a visitor counts at all
	inline bool Eligible(uint32_t visitorAccount, uint32_t ownerAccount, const std::set<uint32_t>& linkedToOwner, bool visitorIsStaff, const Params& params) {
		if (visitorAccount == 0 || visitorAccount == ownerAccount) return false;
		if (params.ignoreLinked && linkedToOwner.contains(visitorAccount)) return false;
		if (params.ignoreStaff && visitorIsStaff) return false;
		return true;
	}

	// How much a repeat visitor's minutes are worth: previousDays is how many other recent days they gave reputation
	inline double RepeatFactor(uint32_t previousDays, double falloff) {
		return 1.0 / (1.0 + std::max(falloff, 0.0) * previousDays);
	}

	inline bool Moved(const Position& a, const Position& b, float minDistance = MIN_MOVE) {
		const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		return dx * dx + dy * dy + dz * dz >= minDistance * minDistance;
	}

	// One visitor's visit, and what they gave the property today
	struct Visit {
		int64_t enteredAt{};
		uint32_t day{};          // UTC day the counters below are for
		int64_t creditedMinutes{};
		double carry{};
		double repeatFactor{ 1.0 };
		int64_t visitorToday{};  // points this visitor's account gave this property today
	};

	/**
	 * A minute of a visit went by. propertyToday: the points the property got today from everyone (updated). Returns
	 * the whole points to give now. A new day restarts the visitor's daily count (the caller resets propertyToday).
	 */
	inline int64_t OnMinute(Visit& visit, const Params& params, int32_t perMinute, int64_t now, uint32_t day, bool active, int64_t& propertyToday) {
		if (day != visit.day) {
			visit.day = day;
			visit.visitorToday = 0;
		}
		if (perMinute <= 0 || params.multiplier <= 0.0) return 0;
		if (now - visit.enteredAt < params.minVisitSeconds) return 0;
		if (params.requireActivity && !active) return 0;
		if (visit.creditedMinutes >= params.maxMinutesPerVisit) return 0;
		visit.creditedMinutes++;

		visit.carry += perMinute * params.multiplier * visit.repeatFactor;
		const auto whole = static_cast<int64_t>(std::floor(visit.carry));
		visit.carry -= static_cast<double>(whole);
		const auto room = std::min(std::max<int64_t>(params.visitorDailyCap - visit.visitorToday, 0), std::max<int64_t>(params.propertyDailyCap - propertyToday, 0));
		const auto granted = std::min(whole, room);
		visit.visitorToday += granted;
		propertyToday += granted;
		return granted;
	}
}

#endif  //!__PROPERTYREPUTATIONRULES__H__
