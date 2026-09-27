#ifndef __PROPERTYRENTRULES__H__
#define __PROPERTYRENTRULES__H__

#include <algorithm>
#include <cstdint>
#include <optional>

/**
 * Property rent (issue #943), the rules without the game around them so they can be unit tested. Off unless
 * property_rent_enabled is on. A property world's rent comes from its PropertyTemplate row (minimumPrice coins every
 * rentDuration x durationType), unless the dashboard's Property Rent page sets another price or period for the world.
 * A price of 0 (Block Yard's) means the property is free.
 *
 * Rent is charged when the owner's character loads in any world. Paying moves the due date one period on from now
 * (missed periods are not charged afterwards). If the owner can't pay, the rent is overdue: after the grace period
 * the property is made private and can't be made public (or best friends only) again until the rent is paid, like
 * live. Nothing on the property changes.
 */
namespace PropertyRentRules {
	constexpr int64_t DAY = 24 * 60 * 60;

	struct Rate {
		int64_t price{};         // coins per period; 0: free
		int64_t periodSeconds{};
		bool operator==(const Rate&) const = default;
	};

	/**
	 * PropertyTemplate's durationType as seconds: 1 is days (the test templates rent for 7 of them) and 6 is months
	 * (every property world rents for 1, and live charged monthly), counted as 30 days. The client only shows the
	 * number (UI_PROPERTY_LEASE_VALUE: "%(every) %(count) %(timeUnits)"), so other values are read as months too.
	 */
	inline int64_t DurationUnit(int32_t durationType) {
		return durationType == 1 ? DAY : 30 * DAY;
	}

	// The template's rent; nullopt when it has none (no price, or no period)
	inline std::optional<Rate> TemplateRate(int64_t minimumPrice, int32_t rentDuration, int32_t durationType) {
		if (minimumPrice <= 0 || rentDuration <= 0) return std::nullopt;
		return Rate{ minimumPrice, static_cast<int64_t>(rentDuration) * DurationUnit(durationType) };
	}

	// A dashboard override (price 0 makes the world free); a period of 0 days keeps the template's
	inline std::optional<Rate> Resolve(const std::optional<Rate>& fromTemplate, std::optional<int64_t> overridePrice, std::optional<int64_t> overrideDays) {
		if (!overridePrice && !overrideDays) return fromTemplate;
		const int64_t price = overridePrice.value_or(fromTemplate ? fromTemplate->price : 0);
		int64_t period = overrideDays && *overrideDays > 0 ? *overrideDays * DAY : (fromTemplate ? fromTemplate->periodSeconds : 30 * DAY);
		if (price <= 0 || period <= 0) return std::nullopt;
		return Rate{ price, period };
	}

	enum class eOutcome : uint8_t {
		NOT_DUE, // nothing to do
		PAID,    // charge `charge` coins; the next rent is due at newDue
		UNPAID,  // due but the owner can't pay; overdue says whether the grace period is over
	};

	struct Decision {
		eOutcome outcome{};
		int64_t charge{};
		int64_t newDue{};
		bool overdue{};
		bool operator==(const Decision&) const = default;
	};

	/**
	 * What to do when the owner loads. due: when the rent is due (0: never charged yet, due now); coins: what the owner
	 * has; grace: seconds after the due date before an unpaid property is made private.
	 */
	inline Decision Decide(const std::optional<Rate>& rate, int64_t due, int64_t now, int64_t coins, int64_t grace) {
		if (!rate || rate->price <= 0) return { eOutcome::NOT_DUE, 0, due, false };
		if (due > now) return { eOutcome::NOT_DUE, 0, due, false };
		if (coins >= rate->price) return { eOutcome::PAID, rate->price, now + rate->periodSeconds, false };
		// Never charged before counts as due from now, so the grace period starts now
		const int64_t dueFrom = due == 0 ? now : due;
		return { eOutcome::UNPAID, 0, dueFrom, now >= dueFrom + std::max<int64_t>(grace, 0) };
	}

	// Whether an unpaid property must be private: rent that was due (a charge was attempted) and the grace period is over
	inline bool IsOverdue(const std::optional<Rate>& rate, int64_t due, int64_t now, int64_t grace) {
		return rate && rate->price > 0 && due > 0 && now >= due + std::max<int64_t>(grace, 0);
	}
}

#endif  //!__PROPERTYRENTRULES__H__
