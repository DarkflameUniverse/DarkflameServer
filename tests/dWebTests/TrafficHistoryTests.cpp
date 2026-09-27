#include <gtest/gtest.h>

#include "TrafficHistory.h"
#include "ServiceType.h"

using namespace TrafficStats;

namespace {
	constexpr int64_t T0 = 1700000040; // a minute starts at 1700000040

	Report Seconds(int64_t from, int64_t count, uint64_t packetsIn, uint64_t bytesOut = 0) {
		Report report;
		for (int64_t t = from; t < from + count; t++) report.seconds.push_back(Second{ .time = t, .packetsIn = packetsIn, .bytesOut = bytesOut });
		return report;
	}

	const uint16_t WORLD = static_cast<uint16_t>(ServiceType::WORLD);
	const uint16_t DASHBOARD = static_cast<uint16_t>(ServiceType::DASHBOARD);
}

TEST(TrafficHistoryTest, Keys) {
	EXPECT_EQ(TrafficHistory::KeyFor(static_cast<uint16_t>(ServiceType::MASTER), 0, 0), "master");
	EXPECT_EQ(TrafficHistory::KeyFor(static_cast<uint16_t>(ServiceType::UGC), 0, 0), "ugc");
	EXPECT_EQ(TrafficHistory::KeyFor(WORLD, 1100, 3), "world:1100:3");
}

TEST(TrafficHistoryTest, SecondsAndBuckets) {
	TrafficHistory h;
	h.Ingest(WORLD, 1100, 1, Seconds(T0, 5, 10, 100), T0 + 5);
	h.Ingest(WORLD, 1100, 1, Seconds(T0 + 5, 5, 20, 100), T0 + 10);
	h.Ingest(DASHBOARD, 0, 0, Seconds(T0, 10, 1), T0 + 10);
	const auto& server = h.Servers().at("world:1100:1");
	EXPECT_EQ(server.seconds.size(), 10u);
	EXPECT_EQ(server.totals.packetsIn, 150u);
	EXPECT_EQ(server.totals.bytesOut, 1000u);

	const auto buckets = h.Buckets(T0, T0 + 10, 5);
	ASSERT_EQ(buckets.size(), 2u);
	const auto& world = buckets.at("world:1100:1");
	ASSERT_EQ(world.size(), 2u);
	EXPECT_EQ(world[0].time, T0);
	EXPECT_EQ(world[0].packetsIn, 50u);
	EXPECT_EQ(world[1].time, T0 + 5);
	EXPECT_EQ(world[1].packetsIn, 100u);
	// A range with nothing leaves the server out
	EXPECT_TRUE(h.Buckets(T0 + 100, T0 + 200, 10).empty());
}

TEST(TrafficHistoryTest, LateAndRepeatedSecondsMerge) {
	TrafficHistory h;
	h.Ingest(WORLD, 1, 1, Seconds(T0 + 2, 2, 1), T0 + 5);
	h.Ingest(WORLD, 1, 1, Seconds(T0, 3, 1), T0 + 5); // T0, T0+1 before, T0+2 again
	const auto& seconds = h.Servers().at("world:1:1").seconds;
	ASSERT_EQ(seconds.size(), 4u);
	EXPECT_EQ(seconds[0].time, T0);
	EXPECT_EQ(seconds[2].time, T0 + 2);
	EXPECT_EQ(seconds[2].packetsIn, 2u);
	for (size_t i = 1; i < seconds.size(); i++) EXPECT_LT(seconds[i - 1].time, seconds[i].time);
}

TEST(TrafficHistoryTest, MinutesAreHandedOutOnceWhenFinal) {
	TrafficHistory h;
	auto report = Seconds(T0, 60, 2);
	report.seconds[10].httpRequests = 4;
	report.seconds[10].httpStatus = { 0, 2, 0, 1, 1 };
	for (int i = 0; i < 4; i++) report.seconds[10].httpLatency.Add(1000 * (i + 1));
	report.link.resends = 7;
	h.Ingest(WORLD, 1, 1, report, T0 + 60);
	h.Ingest(WORLD, 1, 1, Seconds(T0 + 60, 5, 1), T0 + 65);

	// Not final until the slack has passed
	EXPECT_TRUE(h.TakeFinishedMinutes(T0 + 60 + TrafficHistory::MINUTE_SLACK - 1).empty());
	auto rows = h.TakeFinishedMinutes(T0 + 60 + TrafficHistory::MINUTE_SLACK);
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(rows[0].time, T0);
	EXPECT_EQ(rows[0].server, "world:1:1");
	EXPECT_EQ(rows[0].packetsIn, 120u);
	EXPECT_EQ(rows[0].httpRequests, 4u);
	EXPECT_EQ(rows[0].http4xx, 1u);
	EXPECT_EQ(rows[0].http5xx, 1u);
	EXPECT_EQ(rows[0].resends, 7u);
	EXPECT_GT(rows[0].latencyP50Us, 1000u);
	EXPECT_LE(rows[0].latencyP50Us, rows[0].latencyP95Us);
	EXPECT_LE(rows[0].latencyP95Us, rows[0].latencyP99Us);
	EXPECT_TRUE(h.TakeFinishedMinutes(T0 + 60 + TrafficHistory::MINUTE_SLACK).empty());

	// A second of a written minute arriving late isn't written again
	h.Ingest(WORLD, 1, 1, Seconds(T0 + 30, 1, 5), T0 + 100);
	rows = h.TakeFinishedMinutes(T0 + 200);
	ASSERT_EQ(rows.size(), 1u);
	EXPECT_EQ(rows[0].time, T0 + 60);
	EXPECT_EQ(rows[0].packetsIn, 5u);
}

TEST(TrafficHistoryTest, TopMessagesAndRoutes) {
	TrafficHistory h;
	Report a = Seconds(T0, 1, 0);
	a.messages = { MessageCount{ MessageKey{ false, 4, 5, 100 }, 10, 1000 }, MessageCount{ MessageKey{ true, 5, 12, 0 }, 3, 30 } };
	RouteStats route{ .route = "GET /api/x", .count = 2, .bytesOut = 10 };
	route.status[1] = 2;
	route.latency.Add(500, 2);
	a.routes = { route };
	h.Ingest(WORLD, 1, 1, a, T0 + 1);
	Report b = Seconds(T0, 1, 0);
	b.messages = { MessageCount{ MessageKey{ false, 4, 5, 100 }, 5, 500 } };
	b.routes = { route };
	h.Ingest(DASHBOARD, 0, 0, b, T0 + 2);

	const auto all = h.TopMessages("", T0 - 60, 10);
	ASSERT_EQ(all.size(), 2u);
	EXPECT_EQ(all[0].count, 15u);
	EXPECT_EQ(all[0].bytes, 1500u);
	EXPECT_EQ(h.TopMessages("dashboard", T0 - 60, 10).size(), 1u);
	EXPECT_EQ(h.Servers().at("world:1:1").totals.messages.size(), 2u);

	const auto routes = h.Routes(T0 - 60);
	ASSERT_EQ(routes.size(), 2u);
	EXPECT_EQ(routes[0].second.count, 2u);
	EXPECT_EQ(routes[0].second.latency.Count(), 2u);
	EXPECT_TRUE(h.Routes(T0 + 3600).empty());
}

TEST(TrafficHistoryTest, OldSecondsAndSilentServersGo) {
	TrafficHistory h;
	h.Ingest(WORLD, 1, 1, Seconds(T0, 10, 1), T0 + 10);
	h.Ingest(WORLD, 1, 2, Seconds(T0, 10, 1), T0 + 10);
	h.Ingest(WORLD, 1, 2, Seconds(T0 + TrafficHistory::SECONDS_KEPT + 20, 5, 1), T0 + TrafficHistory::SECONDS_KEPT + 25);
	EXPECT_EQ(h.Servers().at("world:1:2").seconds.size(), 5u);
	h.TakeFinishedMinutes(T0 + TrafficHistory::FORGET_AFTER + 100);
	h.Forget(T0 + TrafficHistory::FORGET_AFTER + 100);
	EXPECT_FALSE(h.Servers().contains("world:1:1"));
	EXPECT_TRUE(h.Servers().contains("world:1:2"));
}

TEST(TrafficHistoryTest, ClockFarOffIsIgnored) {
	TrafficHistory h;
	h.Ingest(WORLD, 1, 1, Seconds(T0 + 1000, 5, 1), T0);
	EXPECT_TRUE(h.Servers().at("world:1:1").seconds.empty());
	EXPECT_EQ(h.Servers().at("world:1:1").totals.packetsIn, 0u);
}
