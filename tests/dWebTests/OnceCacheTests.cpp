#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "OnceCache.h"

TEST(OnceCacheTests, BuildsOnceForManyThreads) {
	OnceCache<int, std::string> cache;
	std::atomic<int> builds{};
	std::vector<std::thread> threads;
	std::vector<std::string> results(16);
	for (size_t i = 0; i < results.size(); i++) {
		threads.emplace_back([&, i] {
			results[i] = cache.Get(1, [&] {
				builds++;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				return std::string("built");
			});
		});
	}
	for (auto& thread : threads) thread.join();
	EXPECT_EQ(builds.load(), 1);
	for (const auto& result : results) EXPECT_EQ(result, "built");
	EXPECT_TRUE(cache.Ready(1));
	EXPECT_FALSE(cache.Ready(2));
}

TEST(OnceCacheTests, NotReadyWhileBuilding) {
	OnceCache<int, int> cache;
	std::atomic<bool> release{}, started{};
	std::thread builder([&] {
		cache.Get(1, [&] {
			started = true;
			while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
			return 5;
		});
	});
	while (!started) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_FALSE(cache.Ready(1));
	release = true;
	builder.join();
	EXPECT_TRUE(cache.Ready(1));
	EXPECT_EQ(cache.Get(1, [] { return 0; }), 5);
}

TEST(OnceCacheTests, FailedBuildIsTriedAgain) {
	OnceCache<int, int> cache;
	EXPECT_THROW(cache.Get(1, []() -> int { throw std::runtime_error("no"); }), std::runtime_error);
	EXPECT_FALSE(cache.Ready(1));
	EXPECT_EQ(cache.Get(1, [] { return 3; }), 3);
}
