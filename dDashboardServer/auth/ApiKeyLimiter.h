#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>

/**
 * Per API key request limits: a rate (requests a minute, as a token bucket that refills evenly, so a key can burst up
 * to its whole minute and then goes at the steady rate) and an optional quota of requests per UTC day. Only accepted
 * requests use up the rate and the quota. Kept in memory; the day count is seeded from the database when a key is
 * first seen, so a restart doesn't hand out a fresh quota.
 */
class ApiKeyLimiter {
public:
	using Clock = std::chrono::steady_clock;

	struct Decision {
		bool allowed{};
		bool quotaExceeded{};          // refused by the daily quota rather than the rate
		uint32_t limit{};              // requests a minute
		uint32_t remaining{};          // left in the current minute's allowance
		uint32_t retryAfterSeconds{};  // when refused
		uint32_t quota{};              // requests a day, 0: none
		uint32_t quotaRemaining{};
		uint32_t dayCount{};           // accepted requests today, after this one
	};

	/**
	 * @param day The UTC day (days since 1970) of the request
	 * @param secondsToNextDay Seconds until that day ends (for Retry-After once the quota is used up)
	 * @param storedDay, storedDayCount What the database last recorded for the key, used when it is first seen
	 */
	Decision Check(uint64_t keyId, uint32_t perMinute, uint32_t dailyQuota, int32_t day, uint32_t secondsToNextDay,
		int32_t storedDay, uint32_t storedDayCount, Clock::time_point now) {
		perMinute = std::max<uint32_t>(perMinute, 1);
		auto [it, inserted] = m_Keys.try_emplace(keyId);
		auto& state = it->second;
		if (inserted) {
			state.tokens = perMinute;
			state.refilledAt = now;
			state.day = storedDay;
			state.dayCount = storedDay == day ? storedDayCount : 0;
		}
		if (state.day != day) {
			state.day = day;
			state.dayCount = 0;
		}

		// Refill at perMinute a minute, up to one minute's worth (a lowered limit takes effect at once)
		const double elapsed = std::chrono::duration<double>(now - state.refilledAt).count();
		state.tokens = std::min<double>(perMinute, state.tokens + elapsed * perMinute / 60.0);
		state.refilledAt = now;

		Decision decision;
		decision.limit = perMinute;
		decision.quota = dailyQuota;
		if (dailyQuota > 0 && state.dayCount >= dailyQuota) {
			decision.quotaExceeded = true;
			decision.retryAfterSeconds = std::max<uint32_t>(secondsToNextDay, 1);
		} else if (state.tokens < 1.0) {
			decision.retryAfterSeconds = static_cast<uint32_t>((1.0 - state.tokens) * 60.0 / perMinute) + 1;
		} else {
			state.tokens -= 1.0;
			state.dayCount++;
			decision.allowed = true;
		}
		decision.remaining = static_cast<uint32_t>(state.tokens);
		decision.dayCount = state.dayCount;
		decision.quotaRemaining = dailyQuota > state.dayCount ? dailyQuota - state.dayCount : 0;
		return decision;
	}

	// A revoked or rotated key starts over
	void Forget(uint64_t keyId) { m_Keys.erase(keyId); }

	// Drop keys not used for a while, so memory stays bounded
	void Prune(Clock::time_point now, std::chrono::seconds idle) {
		std::erase_if(m_Keys, [&](const auto& entry) { return now - entry.second.refilledAt > idle; });
	}

	size_t Size() const { return m_Keys.size(); }

private:
	struct State {
		double tokens{};
		Clock::time_point refilledAt;
		int32_t day{};
		uint32_t dayCount{};
	};
	std::unordered_map<uint64_t, State> m_Keys;
};
