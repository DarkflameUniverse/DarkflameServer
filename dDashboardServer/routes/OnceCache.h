#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <mutex>

/**
 * A value per key, built the first time it is asked for and kept: for zone data built from the client's files (a
 * zone's terrain, its scene objects), which worker threads build while the web thread may ask for it too. Any thread.
 * Asked for again while it is being built, the caller waits for that build instead of starting another. A build that
 * throws is forgotten (the next caller tries again) and the exception reaches everyone waiting for it.
 */
template<typename Key, typename Value>
class OnceCache {
public:
	// Whether the value is built (never waits)
	bool Ready(const Key& key) const {
		std::lock_guard lock(m_Mutex);
		const auto it = m_Entries.find(key);
		return it != m_Entries.end() && it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}

	/**
	 * The value, built with `build` when nobody has yet. The reference stays valid: values are never replaced or
	 * removed once built.
	 */
	const Value& Get(const Key& key, const std::function<Value()>& build) {
		std::shared_future<Value> future;
		std::promise<Value> promise;
		bool mine = false;
		{
			std::lock_guard lock(m_Mutex);
			const auto it = m_Entries.find(key);
			if (it != m_Entries.end()) {
				future = it->second;
			} else {
				future = promise.get_future().share();
				m_Entries.emplace(key, future);
				mine = true;
			}
		}
		if (mine) {
			try {
				promise.set_value(build());
			} catch (...) {
				{
					std::lock_guard lock(m_Mutex);
					m_Entries.erase(key);
				}
				promise.set_exception(std::current_exception());
			}
		}
		// The map keeps its own copy of the future, so what get() refers to outlives this call (unless the build threw,
		// in which case get() throws)
		return WaitFor(key, future);
	}

	size_t Size() const {
		std::lock_guard lock(m_Mutex);
		return m_Entries.size();
	}

private:
	const Value& WaitFor(const Key& key, const std::shared_future<Value>& future) {
		future.wait();
		std::lock_guard lock(m_Mutex);
		const auto it = m_Entries.find(key);
		if (it == m_Entries.end()) {
			future.get(); // the build failed: rethrows its exception
			std::terminate(); // unreachable: a successful build is never removed
		}
		return it->second.get();
	}

	mutable std::mutex m_Mutex;
	std::map<Key, std::shared_future<Value>> m_Entries;
};
