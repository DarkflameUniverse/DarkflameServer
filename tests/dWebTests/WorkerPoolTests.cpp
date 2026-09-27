#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "WorkerPool.h"

using ePriority = WorkerPool::ePriority;

TEST(WorkerPoolTests, PickTakesTheMostUrgent) {
	EXPECT_EQ(WorkerPool::Pick({ 0, 1, 1, 1 }, false, 0, 1), ePriority::NORMAL);
	EXPECT_EQ(WorkerPool::Pick({ 2, 1, 1, 1 }, false, 0, 1), ePriority::URGENT);
	EXPECT_EQ(WorkerPool::Pick({ 0, 0, 3, 1 }, false, 0, 1), ePriority::LARGE);
	EXPECT_EQ(WorkerPool::Pick({ 0, 0, 0, 1 }, false, 0, 1), ePriority::BACKGROUND);
	EXPECT_EQ(WorkerPool::Pick({ 0, 0, 0, 0 }, false, 0, 1), std::nullopt);
}

TEST(WorkerPoolTests, FastLaneOnlyTakesUrgentWork) {
	EXPECT_EQ(WorkerPool::Pick({ 1, 1, 1, 1 }, true, 0, 1), ePriority::URGENT);
	EXPECT_EQ(WorkerPool::Pick({ 0, 1, 1, 1 }, true, 0, 1), std::nullopt);
}

TEST(WorkerPoolTests, BackgroundWorkIsLimited) {
	EXPECT_EQ(WorkerPool::Pick({ 0, 0, 0, 5 }, false, 1, 1), std::nullopt);
	EXPECT_EQ(WorkerPool::Pick({ 0, 0, 0, 5 }, false, 1, 2), ePriority::BACKGROUND);
	// Other work still goes ahead
	EXPECT_EQ(WorkerPool::Pick({ 0, 1, 0, 5 }, false, 1, 1), ePriority::NORMAL);
}

TEST(WorkerPoolTests, DefaultThreads) {
	EXPECT_EQ(WorkerPool::DefaultThreads(0), 2u);
	EXPECT_EQ(WorkerPool::DefaultThreads(2), 2u);
	EXPECT_EQ(WorkerPool::DefaultThreads(6), 3u);
	EXPECT_EQ(WorkerPool::DefaultThreads(32), 4u);
}

TEST(WorkerPoolTests, WithoutThreadsJobsRunRightAway) {
	WorkerPool pool;
	bool ran = false;
	pool.Submit(ePriority::NORMAL, [&ran] { ran = true; });
	EXPECT_TRUE(ran);
}

TEST(WorkerPoolTests, RunsEveryJob) {
	WorkerPool pool;
	pool.Start(3);
	std::atomic<int> count{};
	for (int i = 0; i < 200; i++) pool.Submit(static_cast<ePriority>(i % 4), [&count] { count++; });
	pool.WaitIdle();
	EXPECT_EQ(count.load(), 200);
	EXPECT_EQ(pool.Queued(), 0u);
}

// With every general thread busy on a big job, an urgent one still runs in the fast lane
TEST(WorkerPoolTests, UrgentWorkDoesNotWaitBehindBigJobs) {
	WorkerPool pool;
	pool.Start(2);
	std::atomic<bool> release{}, bigStarted{}, urgentDone{};
	pool.Submit(ePriority::LARGE, [&] {
		bigStarted = true;
		while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	while (!bigStarted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	pool.Submit(ePriority::URGENT, [&] { urgentDone = true; });
	for (int i = 0; i < 2000 && !urgentDone; i++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_TRUE(urgentDone.load());
	release = true;
	pool.WaitIdle();
}

TEST(WorkerPoolTests, OrderWithinAndAcrossPriorities) {
	WorkerPool pool;
	pool.Start(2);
	std::mutex mutex;
	std::vector<std::string> order;
	std::atomic<bool> release{}, blockerStarted{};
	// Occupy the general thread so the rest queue up
	pool.Submit(ePriority::NORMAL, [&] {
		blockerStarted = true;
		while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	while (!blockerStarted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const auto job = [&](std::string name) { return [&, name] { std::lock_guard lock(mutex); order.push_back(name); }; };
	pool.Submit(ePriority::BACKGROUND, job("background"));
	pool.Submit(ePriority::LARGE, job("large"));
	pool.Submit(ePriority::NORMAL, job("normal 1"));
	pool.Submit(ePriority::NORMAL, job("normal 2"));
	pool.Submit(ePriority::NORMAL, job("normal 0"), 0, true);
	release = true;
	pool.WaitIdle();
	EXPECT_EQ(order, (std::vector<std::string>{ "normal 0", "normal 1", "normal 2", "large", "background" }));
}

TEST(WorkerPoolTests, CancelDropsAGroupsQueuedJobs) {
	WorkerPool pool;
	pool.Start(2);
	std::atomic<bool> release{}, blockerStarted{};
	std::atomic<int> ran{};
	pool.Submit(ePriority::NORMAL, [&] {
		blockerStarted = true;
		while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	while (!blockerStarted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	for (int i = 0; i < 5; i++) pool.Submit(ePriority::BACKGROUND, [&ran] { ran++; }, 42);
	pool.Submit(ePriority::BACKGROUND, [&ran] { ran += 100; }, 7);
	EXPECT_EQ(pool.Cancel(42), 5u);
	EXPECT_EQ(pool.Cancel(0), 0u);
	release = true;
	pool.WaitIdle();
	EXPECT_EQ(ran.load(), 100);
}

TEST(WorkerPoolTests, StopDropsQueuedJobs) {
	WorkerPool pool;
	pool.Start(2);
	std::atomic<bool> release{}, blockerStarted{};
	std::atomic<int> ran{};
	pool.Submit(ePriority::NORMAL, [&] {
		blockerStarted = true;
		while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	});
	while (!blockerStarted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	for (int i = 0; i < 5; i++) pool.Submit(ePriority::NORMAL, [&ran] { ran++; });
	std::thread stopper([&pool] { pool.Stop(); });
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	release = true;
	stopper.join();
	EXPECT_EQ(ran.load(), 0);
	EXPECT_FALSE(pool.Running());
}
