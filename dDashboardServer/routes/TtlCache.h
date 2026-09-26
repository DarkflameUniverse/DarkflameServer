#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>

/**
 * A small in-memory cache for public pages, so visitors who aren't signed in can't make every request reach the
 * database. Entries expire after `ttl`; when the entries weigh more than `maxWeight` together (e.g. bytes), the
 * oldest are dropped first. Not thread safe: dashboard routes run on the web server's one thread.
 */
template<typename Key, typename Value>
class TtlCache {
public:
	using Clock = std::chrono::steady_clock;

	TtlCache(std::chrono::seconds ttl, size_t maxWeight) : m_Ttl(ttl), m_MaxWeight(maxWeight) {}

	std::optional<Value> Get(const Key& key, Clock::time_point now = Clock::now()) {
		const auto it = m_Entries.find(key);
		if (it == m_Entries.end()) return std::nullopt;
		if (now - it->second.stored >= m_Ttl) {
			Remove(it);
			return std::nullopt;
		}
		return it->second.value;
	}

	// Store a value; weight 1 by default, so maxWeight is then a number of entries. Heavier than maxWeight: not kept.
	void Put(const Key& key, Value value, size_t weight = 1, Clock::time_point now = Clock::now()) {
		if (const auto it = m_Entries.find(key); it != m_Entries.end()) Remove(it);
		if (weight > m_MaxWeight) return;
		while (!m_Entries.empty() && m_Weight + weight > m_MaxWeight) Remove(Oldest());
		m_Entries.emplace(key, Entry{ std::move(value), now, weight });
		m_Weight += weight;
	}

	void SetTtl(std::chrono::seconds ttl) { m_Ttl = ttl; }
	size_t Size() const { return m_Entries.size(); }
	size_t Weight() const { return m_Weight; }

private:
	struct Entry {
		Value value;
		Clock::time_point stored;
		size_t weight{};
	};
	using Iterator = typename std::map<Key, Entry>::iterator;

	Iterator Oldest() {
		auto oldest = m_Entries.begin();
		for (auto it = m_Entries.begin(); it != m_Entries.end(); ++it) if (it->second.stored < oldest->second.stored) oldest = it;
		return oldest;
	}

	void Remove(Iterator it) {
		m_Weight -= it->second.weight;
		m_Entries.erase(it);
	}

	std::chrono::seconds m_Ttl;
	size_t m_MaxWeight;
	size_t m_Weight{};
	std::map<Key, Entry> m_Entries;
};
