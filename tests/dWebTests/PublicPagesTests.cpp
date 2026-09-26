#include <gtest/gtest.h>

#include "PublicStatus.h"
#include "TtlCache.h"
#include "ILeaderboard.h"

using namespace PublicStatus;

namespace {
	constexpr int64_t BUCKET = 300;
	constexpr int64_t DAY = 86400;

	// Up (or down) buckets from `from` to before `to`
	void Fill(std::vector<IServerHealth::HealthSample>& samples, int64_t from, int64_t to, bool up) {
		for (int64_t time = from; time < to; time += BUCKET) {
			IServerHealth::HealthSample sample;
			sample.time = time;
			sample.authOnline = up;
			samples.push_back(sample);
		}
	}
}

TEST(PublicStatusTests, NoHistoryMeansUnknown) {
	const auto uptime = Summarize({}, 1'000'000, BUCKET);
	EXPECT_EQ(uptime.upSince, 0);
	EXPECT_LT(uptime.availability24h, 0);
	EXPECT_LT(uptime.availability7d, 0);
}

TEST(PublicStatusTests, UpSinceTheLastGapOrOutage) {
	const int64_t now = 100 * DAY + 150; // part way into a bucket
	const int64_t current = now / BUCKET * BUCKET;
	std::vector<IServerHealth::HealthSample> samples;
	Fill(samples, current - DAY, current - DAY / 2, true);
	Fill(samples, current - DAY / 2, current - DAY / 2 + BUCKET, false); // one bucket down
	Fill(samples, current - DAY / 2 + BUCKET, current + BUCKET, true);
	auto uptime = Summarize(samples, now, BUCKET);
	EXPECT_EQ(uptime.upSince, current - DAY / 2 + BUCKET);

	// A gap in the samples (the dashboard wasn't running) ends the run too
	samples.clear();
	Fill(samples, current - DAY, current - 10 * BUCKET, true);
	Fill(samples, current - 5 * BUCKET, current + BUCKET, true);
	uptime = Summarize(samples, now, BUCKET);
	EXPECT_EQ(uptime.upSince, current - 5 * BUCKET);
}

TEST(PublicStatusTests, DownWhenTheNewestSampleIsDownOrOld) {
	const int64_t now = 100 * DAY;
	std::vector<IServerHealth::HealthSample> samples;
	Fill(samples, now - DAY, now - BUCKET, true);
	Fill(samples, now - BUCKET, now, false);
	EXPECT_EQ(Summarize(samples, now, BUCKET).upSince, 0);

	samples.clear();
	Fill(samples, now - DAY, now - 3 * BUCKET, true); // nothing for the last 15 minutes
	EXPECT_EQ(Summarize(samples, now, BUCKET).upSince, 0);
}

TEST(PublicStatusTests, AvailabilityCountsMissingBucketsAsDown) {
	const int64_t now = 100 * DAY;
	std::vector<IServerHealth::HealthSample> samples;
	// A server that started two days ago and was down (no samples) for a quarter of the last day
	Fill(samples, now - 2 * DAY, now - DAY / 2, true);
	Fill(samples, now - DAY / 4, now, true);
	const auto uptime = Summarize(samples, now, BUCKET);
	EXPECT_NEAR(uptime.availability24h, 0.75, 0.01);
	// Only counted from its first sample, not from a week ago
	EXPECT_NEAR(uptime.availability7d, 1.75 / 2.0, 0.01);
}

TEST(PublicStatusTests, ZonesAreAddedUpBusiestFirst) {
	const auto zones = CountByZone({ {1200, "Nimbus Station", 3}, {1000, "Venture Explorer", 1}, {1200, "Nimbus Station", 4}, {1100, "Avant Gardens", 0} });
	ASSERT_EQ(zones.size(), 3u);
	EXPECT_EQ(zones[0].zoneId, 1200u);
	EXPECT_EQ(zones[0].players, 7u);
	EXPECT_EQ(zones[0].instances, 2u);
	EXPECT_EQ(zones[1].zoneId, 1000u);
	EXPECT_EQ(zones[2].players, 0u);
}

TEST(PublicStatusTests, DurationsUseTheLargestUnit) {
	EXPECT_EQ(DurationText(30), "a moment");
	EXPECT_EQ(DurationText(60), "1 minute");
	EXPECT_EQ(DurationText(3 * 3600 + 59), "3 hours");
	EXPECT_EQ(DurationText(86400), "1 day");
	EXPECT_EQ(DurationText(10 * 86400 + 5), "10 days");
}

TEST(TtlCacheTests, EntriesExpire) {
	using Clock = std::chrono::steady_clock;
	TtlCache<int, std::string> cache(std::chrono::seconds(10), 100);
	const auto start = Clock::now();
	cache.Put(1, "one", 1, start);
	EXPECT_EQ(cache.Get(1, start + std::chrono::seconds(9)), "one");
	EXPECT_FALSE(cache.Get(1, start + std::chrono::seconds(10)).has_value());
	EXPECT_EQ(cache.Size(), 0u);
}

TEST(TtlCacheTests, OldestGoFirstWhenTooHeavy) {
	using Clock = std::chrono::steady_clock;
	TtlCache<int, int> cache(std::chrono::seconds(60), 10);
	const auto start = Clock::now();
	cache.Put(1, 1, 4, start);
	cache.Put(2, 2, 4, start + std::chrono::seconds(1));
	cache.Put(3, 3, 4, start + std::chrono::seconds(2)); // 12 > 10: the oldest goes
	EXPECT_FALSE(cache.Get(1, start + std::chrono::seconds(3)).has_value());
	EXPECT_EQ(cache.Get(2, start + std::chrono::seconds(3)), 2);
	EXPECT_EQ(cache.Weight(), 8u);
	// Replacing an entry doesn't count it twice; something heavier than the whole cache isn't kept
	cache.Put(2, 20, 4, start + std::chrono::seconds(4));
	EXPECT_EQ(cache.Weight(), 8u);
	cache.Put(4, 4, 11, start + std::chrono::seconds(5));
	EXPECT_FALSE(cache.Get(4, start + std::chrono::seconds(5)).has_value());
	EXPECT_EQ(cache.Size(), 2u);
}

// Leaderboards rank the way the game does (shared by dGame's LeaderboardManager and the dashboard)
TEST(LeaderboardRankingTests, OnlyRacesRankLowerFirst) {
	EXPECT_TRUE(ILeaderboard::LowerIsBetter(eLeaderboardType::Racing));
	EXPECT_TRUE(ILeaderboard::LowerIsBetter(eLeaderboardType::MonumentRace)); // time taken
	EXPECT_FALSE(ILeaderboard::LowerIsBetter(eLeaderboardType::FootRace));    // time left on the countdown
	EXPECT_FALSE(ILeaderboard::LowerIsBetter(eLeaderboardType::ShootingGallery));
	EXPECT_FALSE(ILeaderboard::LowerIsBetter(eLeaderboardType::Donations));
}
