#include "WorkerPool.h"

#include <algorithm>
#include <exception>

#include "Game.h"
#include "Logger.h"

std::optional<WorkerPool::ePriority> WorkerPool::Pick(const std::array<size_t, PRIORITIES>& queued, bool fastLane, size_t runningBackground, size_t maxBackground) {
	for (size_t i = 0; i < PRIORITIES; i++) {
		const auto priority = static_cast<ePriority>(i);
		if (fastLane && priority != ePriority::URGENT) break;
		if (priority == ePriority::BACKGROUND && runningBackground >= maxBackground) break;
		if (queued[i] > 0) return priority;
	}
	return std::nullopt;
}

size_t WorkerPool::DefaultThreads(size_t cores) {
	return std::clamp<size_t>(cores / 2, 2, 4);
}

void WorkerPool::Start(size_t threads, size_t maxBackground) {
	Stop();
	std::lock_guard lock(m_Mutex);
	m_Stopping = false;
	m_MaxBackground = std::max<size_t>(1, maxBackground);
	threads = std::max<size_t>(2, threads);
	for (size_t i = 0; i < threads; i++) m_Threads.emplace_back([this, i] { Work(i == 0); });
}

void WorkerPool::Stop() {
	std::vector<std::thread> threads;
	{
		std::lock_guard lock(m_Mutex);
		m_Stopping = true;
		for (auto& queue : m_Queues) queue.clear();
		threads.swap(m_Threads);
	}
	m_Wake.notify_all();
	for (auto& thread : threads) thread.join();
	m_Idle.notify_all();
}

void WorkerPool::Submit(ePriority priority, Job job, uint64_t group, bool front) {
	if (!job) return;
	{
		std::lock_guard lock(m_Mutex);
		if (!m_Threads.empty()) {
			auto& queue = m_Queues[static_cast<size_t>(priority)];
			Entry entry{ std::move(job), group };
			if (front) queue.push_front(std::move(entry));
			else queue.push_back(std::move(entry));
			job = nullptr;
		}
	}
	if (!job) {
		// Wake them all: the one woken might be the fast lane, which can't take this
		m_Wake.notify_all();
		return;
	}
	try {
		job();
	} catch (const std::exception& ex) {
		LOG("A background job failed: %s", ex.what());
	}
}

size_t WorkerPool::Cancel(uint64_t group) {
	if (group == 0) return 0;
	size_t dropped = 0;
	{
		std::lock_guard lock(m_Mutex);
		for (auto& queue : m_Queues) {
			const auto before = queue.size();
			std::erase_if(queue, [group](const Entry& entry) { return entry.group == group; });
			dropped += before - queue.size();
		}
	}
	m_Idle.notify_all();
	return dropped;
}

size_t WorkerPool::Queued() const {
	std::lock_guard lock(m_Mutex);
	size_t total = 0;
	for (const auto& queue : m_Queues) total += queue.size();
	return total;
}

size_t WorkerPool::Active() const {
	std::lock_guard lock(m_Mutex);
	return m_Active;
}

void WorkerPool::WaitIdle() {
	std::unique_lock lock(m_Mutex);
	m_Idle.wait(lock, [this] {
		if (m_Active > 0) return false;
		return std::all_of(m_Queues.begin(), m_Queues.end(), [](const auto& queue) { return queue.empty(); });
	});
}

void WorkerPool::Work(bool fastLane) {
	std::unique_lock lock(m_Mutex);
	while (true) {
		std::optional<ePriority> priority;
		m_Wake.wait(lock, [&] {
			if (m_Stopping) return true;
			std::array<size_t, PRIORITIES> queued{};
			for (size_t i = 0; i < PRIORITIES; i++) queued[i] = m_Queues[i].size();
			priority = Pick(queued, fastLane, m_RunningBackground, m_MaxBackground);
			return priority.has_value();
		});
		if (m_Stopping) return;

		auto& queue = m_Queues[static_cast<size_t>(*priority)];
		auto entry = std::move(queue.front());
		queue.pop_front();
		const bool background = *priority == ePriority::BACKGROUND;
		if (background) m_RunningBackground++;
		m_Active++;
		lock.unlock();

		try {
			entry.job();
		} catch (const std::exception& ex) {
			LOG("A background job failed: %s", ex.what());
		}
		entry.job = nullptr; // what it holds is released outside the lock

		lock.lock();
		m_Active--;
		if (background) {
			m_RunningBackground--;
			m_Wake.notify_all(); // another background job may start now
		}
		m_Idle.notify_all();
	}
}
