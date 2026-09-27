#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

/**
 * A few worker threads for slow work that routes hand off with Web::Defer (converting the client's models), so the
 * web server's one thread keeps answering. Jobs run by priority, in the order they came within one.
 *
 * The first thread is a fast lane that only runs URGENT jobs, so something small and wanted now (a flair near the
 * camera) never waits behind big conversions filling the other threads. BACKGROUND jobs (converting ahead of time)
 * only run when nothing else waits, and only a few at once, so they never take every thread.
 */
class WorkerPool {
public:
	enum class ePriority : uint8_t {
		URGENT,     // small and wanted now: the fast lane takes these too
		NORMAL,
		LARGE,      // big jobs someone waits for
		BACKGROUND, // nobody waits for it
	};
	static constexpr size_t PRIORITIES = 4;

	using Job = std::function<void()>;

	WorkerPool() = default;
	~WorkerPool() { Stop(); }
	WorkerPool(const WorkerPool&) = delete;
	WorkerPool& operator=(const WorkerPool&) = delete;

	/**
	 * Start `threads` workers (at least 2: the fast lane and one more). At most `maxBackground` BACKGROUND jobs run
	 * at once (at least 1).
	 */
	void Start(size_t threads, size_t maxBackground = 1);

	// Drop the queued jobs and wait for the running ones
	void Stop();

	bool Running() const { return !m_Threads.empty(); }
	size_t Threads() const { return m_Threads.size(); }

	/**
	 * Queue a job. `group` (0 for none) lets Cancel drop jobs queued together; `front` puts it before the others of
	 * its priority. Without threads (not started) the job runs right away on the caller's thread.
	 */
	void Submit(ePriority priority, Job job, uint64_t group = 0, bool front = false);

	// Drop the queued jobs of `group`; returns how many
	size_t Cancel(uint64_t group);

	size_t Queued() const;
	size_t Active() const;

	// Wait until nothing is queued or running (for tests)
	void WaitIdle();

	/**
	 * Which priority a worker takes next from queues of these lengths: the most urgent one waiting, only URGENT for
	 * the fast lane, and BACKGROUND only while fewer than maxBackground of those run. nullopt: nothing for it.
	 */
	static std::optional<ePriority> Pick(const std::array<size_t, PRIORITIES>& queued, bool fastLane, size_t runningBackground, size_t maxBackground);

	/**
	 * The default number of threads for this many CPU cores: half of them, from 2 to 4 (a conversion is one core's
	 * work, and the game servers on the same machine need the rest)
	 */
	static size_t DefaultThreads(size_t cores);

private:
	struct Entry {
		Job job;
		uint64_t group{};
	};

	void Work(bool fastLane);

	mutable std::mutex m_Mutex;
	std::condition_variable m_Wake;
	std::condition_variable m_Idle;
	std::array<std::deque<Entry>, PRIORITIES> m_Queues;
	std::vector<std::thread> m_Threads;
	size_t m_MaxBackground{ 1 };
	size_t m_RunningBackground{};
	size_t m_Active{};
	bool m_Stopping{};
};
