#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>

/**
 * Request limits for the AI moderator helper, so the dashboard can't run up a bill: at most `perMinute` requests in
 * any 60 seconds and `perDay` per UTC day. Pure (times are passed in) so it can be unit tested. Main thread only.
 */
class AiBudget {
public:
	struct Limits {
		uint32_t perMinute{ 5 };
		uint32_t perDay{ 200 };
	};

	enum class eDecision : uint8_t { OK, MINUTE, DAY };

	static int64_t Day(int64_t now) { return now / 86400; }
	static int64_t NextDayStart(int64_t now) { return (Day(now) + 1) * 86400; }

	// Requests already made today (e.g. counted from the database after a restart)
	void Seed(int64_t now, uint32_t usedToday) {
		Roll(now);
		m_DayCount = usedToday;
	}

	// Take one request from the budget if both limits allow it
	eDecision TryAcquire(const Limits& limits, int64_t now) {
		Roll(now);
		if (m_DayCount >= limits.perDay) return eDecision::DAY;
		if (m_Minute.size() >= limits.perMinute) return eDecision::MINUTE;
		m_Minute.push_back(now);
		m_DayCount++;
		return eDecision::OK;
	}

	uint32_t UsedToday(int64_t now) {
		Roll(now);
		return m_DayCount;
	}

	uint32_t RemainingToday(const Limits& limits, int64_t now) {
		const auto used = UsedToday(now);
		return used >= limits.perDay ? 0 : limits.perDay - used;
	}

	// Seconds until the per-minute limit lets another request through (0: now)
	int64_t MinuteWait(const Limits& limits, int64_t now) {
		Roll(now);
		if (m_Minute.size() < limits.perMinute || m_Minute.empty()) return 0;
		return std::max<int64_t>(1, m_Minute.front() + 60 - now);
	}

private:
	void Roll(int64_t now) {
		while (!m_Minute.empty() && now - m_Minute.front() >= 60) m_Minute.pop_front();
		if (Day(now) != m_Day) {
			m_Day = Day(now);
			m_DayCount = 0;
		}
	}

	std::deque<int64_t> m_Minute;
	int64_t m_Day{ -1 };
	uint32_t m_DayCount{};
};
