#pragma once

#include <algorithm>
#include <cstdint>

/**
 * When to start another spare instance of a zone (InstanceManager::KeepSpareInstances). A spare that stops before it
 * has run for STABLE seconds (a crash at start or soon after, a missing file, a bad port) counts as a failure, and each
 * failure in a row doubles the wait before the next try, up to half an hour, so a broken zone isn't restarted every few
 * seconds forever. A spare that keeps running for STABLE seconds resets it. Times are seconds on any steady clock,
 * passed in so it can be tested.
 */
class SpareBackoff {
public:
	static constexpr int64_t BASE_DELAY = 10;
	static constexpr int64_t MAX_DELAY = 30 * 60;
	static constexpr int64_t STABLE = 5 * 60;

	// Whether a new spare may be started now: none of ours is still starting, and any wait has passed
	bool CanStart(int64_t now) const { return (m_Watched == 0 || m_ReadySince != 0) && now >= m_NextAttempt; }

	void Started(uint32_t instanceId) {
		m_Watched = instanceId;
		m_ReadySince = 0;
	}

	// The instance we started is still there; ready says whether it has finished starting
	void Running(bool ready, int64_t now) {
		if (!ready) return;
		if (m_ReadySince == 0) m_ReadySince = now;
		if (now - m_ReadySince >= STABLE) {
			m_Watched = 0;
			m_ReadySince = 0;
			m_Failures = 0;
		}
	}

	// The instance we started went away before running for STABLE seconds
	void Lost(int64_t now) {
		m_Watched = 0;
		m_ReadySince = 0;
		m_Failures++;
		m_NextAttempt = now + Delay(m_Failures);
	}

	uint32_t Watched() const { return m_Watched; }
	uint32_t Failures() const { return m_Failures; }

	static int64_t Delay(uint32_t failures) {
		if (failures == 0) return 0;
		const auto shift = std::min<uint32_t>(failures - 1, 20);
		return std::min<int64_t>(BASE_DELAY << shift, MAX_DELAY);
	}

private:
	uint32_t m_Watched{};
	int64_t m_ReadySince{};
	uint32_t m_Failures{};
	int64_t m_NextAttempt{};
};
