#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>

/**
 * Failed sign-in throttling keyed to (network address, account).
 *
 * Wrong passwords or codes from one address only block that address from that account for a while, so a stranger
 * can't lock the real owner out by guessing. The account-wide lockout in the database (SetLockout/IsLockedOut) is a
 * separate, much higher backstop against guessing from many addresses, and is also time-limited.
 *
 * Not thread-safe; used from the dashboard's web thread only. Times are unix seconds, passed in so it can be tested.
 */
class LoginThrottle {
public:
	LoginThrottle(uint32_t maxFailures, int64_t windowSeconds, int64_t blockSeconds)
		: m_MaxFailures(maxFailures), m_Window(windowSeconds), m_Block(blockSeconds) {}

	// Seconds this address is still blocked from this account, 0 when it may try
	int64_t BlockedFor(const std::string& address, uint32_t accountId, int64_t now) const {
		const auto it = m_Entries.find({ address, accountId });
		if (it == m_Entries.end() || it->second.blockedUntil <= now) return 0;
		return it->second.blockedUntil - now;
	}

	// Count a failure; returns whether the address is now blocked from the account
	bool RecordFailure(const std::string& address, uint32_t accountId, int64_t now) {
		Prune(now);
		auto& entry = m_Entries[{ address, accountId }];
		entry.failures.push_back(now);
		while (!entry.failures.empty() && entry.failures.front() <= now - m_Window) entry.failures.pop_front();
		if (entry.failures.size() >= m_MaxFailures) {
			entry.blockedUntil = now + m_Block;
			entry.failures.clear();
			return true;
		}
		return false;
	}

	// A successful sign-in from this address forgets its failures
	void Clear(const std::string& address, uint32_t accountId) { m_Entries.erase({ address, accountId }); }

	// Staff unlocking an account forgets every address's failures for it
	void ClearAccount(uint32_t accountId) {
		std::erase_if(m_Entries, [accountId](const auto& entry) { return entry.first.second == accountId; });
	}

	size_t Size() const { return m_Entries.size(); }

private:
	struct Entry {
		std::deque<int64_t> failures;
		int64_t blockedUntil{};
	};

	// Drop entries with nothing left to remember so memory stays bounded
	void Prune(int64_t now) {
		if (++m_Calls % 256 != 0) return;
		std::erase_if(m_Entries, [this, now](const auto& entry) {
			const auto& e = entry.second;
			return e.blockedUntil <= now && (e.failures.empty() || e.failures.back() <= now - m_Window);
		});
	}

	uint32_t m_MaxFailures;
	int64_t m_Window;
	int64_t m_Block;
	uint32_t m_Calls{};
	std::map<std::pair<std::string, uint32_t>, Entry> m_Entries;
};
