#include <gtest/gtest.h>

#include "PerfHistory.h"
#include "Permissions.h"
#include "ServiceType.h"

namespace {
	constexpr int64_t NOW = 1700000100;

	size_t P(Profiler::Phase phase) { return static_cast<size_t>(phase); }

	// Five seconds of a world at 30 frames a second, 3 ms each (2 of them packets); one 400 ms frame in the last
	Profiler::Report WorldReport(int64_t end = NOW) {
		Profiler::Report report;
		report.present = true;
		report.slowThresholdMs = 250;
		for (int64_t t = end - 5; t < end; t++) {
			Profiler::Second s{ .time = t, .ticks = 30, .totalUs = 90000, .maxUs = 3000 };
			s.frames.Add(3000, 30);
			s.phaseUs[P(Profiler::Phase::PACKETS)] = 60000;
			s.phaseUs[P(Profiler::Phase::ENTITIES)] = 30000;
			report.seconds.push_back(s);
		}
		auto& last = report.seconds.back();
		last.ticks++;
		last.totalUs += 400000;
		last.maxUs = 400000;
		last.frames.Add(400000);
		last.phaseUs[P(Profiler::Phase::CDCLIENT)] += 400000;

		Profiler::Frame slow{ .timeMs = (end - 1) * 1000, .durationUs = 400000 };
		slow.phaseUs[P(Profiler::Phase::CDCLIENT)] = 390000;
		slow.scopes = {
			{ .name = Profiler::FRAME, .depth = 0, .count = 1, .totalUs = 400000 },
			{ .name = Profiler::PACKET, .arg = 77, .depth = 1, .count = 1, .totalUs = 399000, .startUs = 50 },
			{ .name = "CDClient Objects", .depth = 2, .count = 9800, .totalUs = 390000, .startUs = 60 },
		};
		report.worst = { slow };
		report.slow = { slow };
		report.messages = { { .key = 77, .count = 10, .totalUs = 399500, .maxUs = 399000 }, { .key = 5, .count = 100, .totalUs = 1000, .maxUs = 50 } };
		return report;
	}

	std::string Label(const Profiler::Node& node) {
		return node.name == Profiler::PACKET ? "Packet named " + std::to_string(node.arg) : Profiler::DefaultLabel(node);
	}
	std::string ServerLabel(const std::string& key) { return "label of " + key; }
}

TEST(PerfHistoryTest, KeysParse) {
	uint16_t type{};
	uint32_t zone{}, instance{};
	ASSERT_TRUE(PerfHistory::ParseKey("world:1200:3", type, zone, instance));
	EXPECT_EQ(type, static_cast<uint16_t>(ServiceType::WORLD));
	EXPECT_EQ(zone, 1200u);
	EXPECT_EQ(instance, 3u);
	ASSERT_TRUE(PerfHistory::ParseKey("dashboard", type, zone, instance));
	EXPECT_EQ(type, static_cast<uint16_t>(ServiceType::DASHBOARD));
	EXPECT_EQ(zone, 0u);
	EXPECT_FALSE(PerfHistory::ParseKey("world:12a:3", type, zone, instance));
	EXPECT_FALSE(PerfHistory::ParseKey("world:1200", type, zone, instance));
	EXPECT_FALSE(PerfHistory::ParseKey("service:9:0:0", type, zone, instance));
	EXPECT_FALSE(PerfHistory::ParseKey("", type, zone, instance));
}

TEST(PerfHistoryTest, ReportsWithoutFramesAreIgnored) {
	PerfHistory history;
	history.Ingest("auth", Profiler::Report{}, NOW);
	EXPECT_TRUE(history.Servers().empty());
}

TEST(PerfHistoryTest, ServersSummariseTheirLoops) {
	PerfHistory history;
	history.Ingest("world:1200:3", WorldReport(), NOW);
	const auto servers = history.ServersJson(NOW, 300, 20, ServerLabel);
	ASSERT_EQ(servers.size(), 1u);
	const auto& world = servers[0];
	EXPECT_EQ(world["key"], "world:1200:3");
	EXPECT_EQ(world["label"], "label of world:1200:3");
	EXPECT_TRUE(world["online"].get<bool>());
	EXPECT_EQ(world["seconds"], 5);
	EXPECT_DOUBLE_EQ(world["ticks_per_second"].get<double>(), 30.2);
	EXPECT_DOUBLE_EQ(world["max_ms"].get<double>(), 400.0);
	// 850 ms of frames in 5 seconds
	EXPECT_DOUBLE_EQ(world["busy_percent"].get<double>(), 17.0);
	EXPECT_EQ(world["slow"], 1);
	EXPECT_EQ(world["slow_threshold_ms"], 250);
	EXPECT_FALSE(history.ServersJson(NOW + 100, 300, 20, ServerLabel)[0]["online"].get<bool>());
}

TEST(PerfHistoryTest, SeriesHasFrameTimesAndPhasesPerStep) {
	PerfHistory history;
	history.Ingest("world:1200:3", WorldReport(), NOW);
	const auto series = history.Series("world:1200:3", NOW - 6, NOW, 1);
	ASSERT_EQ(series["times"].size(), 6u);
	// The first second wasn't reported: gaps
	EXPECT_TRUE(series["ticks_per_second"][0].is_null());
	EXPECT_TRUE(series["avg_ms"][0].is_null());
	EXPECT_TRUE(series["phases_ms_per_second"]["packets"][0].is_null());
	EXPECT_DOUBLE_EQ(series["ticks_per_second"][1].get<double>(), 30.0);
	EXPECT_DOUBLE_EQ(series["avg_ms"][1].get<double>(), 3.0);
	EXPECT_DOUBLE_EQ(series["max_ms"][5].get<double>(), 400.0);
	EXPECT_DOUBLE_EQ(series["phases_ms_per_second"]["packets"][1].get<double>(), 60.0);
	EXPECT_DOUBLE_EQ(series["phases_ms_per_second"]["cdclient"][5].get<double>(), 400.0);
	EXPECT_EQ(series["slow_threshold_ms"], 250);

	// Steps of 5 seconds: per-second averages, the longest frame, p95 from the merged histograms
	const auto coarse = history.Series("world:1200:3", NOW - 5, NOW, 5);
	ASSERT_EQ(coarse["times"].size(), 1u);
	EXPECT_DOUBLE_EQ(coarse["ticks_per_second"][0].get<double>(), 30.2);
	EXPECT_DOUBLE_EQ(coarse["max_ms"][0].get<double>(), 400.0);
	EXPECT_LT(coarse["p95_ms"][0].get<double>(), 5.0);
	EXPECT_DOUBLE_EQ(coarse["phases_ms_per_second"]["cdclient"][0].get<double>(), 80.0);
}

TEST(PerfHistoryTest, SecondsFromLateReportsMerge) {
	PerfHistory history;
	history.Ingest("chat", WorldReport(), NOW);
	history.Ingest("chat", WorldReport(), NOW + 1); // the same seconds again
	const auto series = history.Series("chat", NOW - 5, NOW, 1);
	EXPECT_DOUBLE_EQ(series["ticks_per_second"][0].get<double>(), 60.0);
	EXPECT_EQ(history.Servers().at("chat").seconds.size(), 5u);
}

TEST(PerfHistoryTest, WorstSlowAndMessagesAreLabelled) {
	PerfHistory history;
	history.Ingest("world:1200:3", WorldReport(), NOW);
	const auto worst = history.Worst("world:1200:3", NOW - 600, 10, Label);
	ASSERT_EQ(worst.size(), 1u);
	EXPECT_DOUBLE_EQ(worst[0]["duration_ms"].get<double>(), 400.0);
	EXPECT_DOUBLE_EQ(worst[0]["phases"]["cdclient"].get<double>(), 390.0);
	ASSERT_EQ(worst[0]["scopes"].size(), 3u);
	EXPECT_EQ(worst[0]["scopes"][1]["label"], "Packet named 77");
	EXPECT_EQ(worst[0]["scopes"][2]["count"], 9800);
	EXPECT_DOUBLE_EQ(worst[0]["scopes"][1]["self_ms"].get<double>(), 9.0);
	EXPECT_NE(worst[0]["path"].get<std::string>().find("Packet named 77"), std::string::npos);

	const auto slow = history.SlowJson("", Label, ServerLabel);
	ASSERT_EQ(slow.size(), 1u);
	EXPECT_EQ(slow[0]["server"], "world:1200:3");
	EXPECT_EQ(slow[0]["server_label"], "label of world:1200:3");
	EXPECT_TRUE(history.SlowJson("auth", Label, ServerLabel).empty());

	const auto messages = history.Messages("world:1200:3", NOW - 300, 10, [](uint64_t key) { return nlohmann::json{ {"packet", std::to_string(key)} }; });
	ASSERT_EQ(messages.size(), 2u);
	EXPECT_EQ(messages[0]["packet"], "77");
	EXPECT_EQ(messages[0]["count"], 10);
	EXPECT_DOUBLE_EQ(messages[0]["max_ms"].get<double>(), 399.0);
	EXPECT_DOUBLE_EQ(messages[1]["avg_ms"].get<double>(), 0.01);
}

TEST(PerfHistoryTest, SlowFramesKeepTheLastFifty) {
	PerfHistory history;
	for (int i = 0; i < 60; i++) history.Ingest("world:1200:" + std::to_string(i % 3), WorldReport(NOW + i * 5), NOW + i * 5);
	EXPECT_EQ(history.Slow().size(), PerfHistory::SLOW_KEPT);
	// Newest first
	const auto slow = history.SlowJson("", Label, ServerLabel);
	EXPECT_GT(slow[0]["time_ms"].get<int64_t>(), slow[1]["time_ms"].get<int64_t>());
}

TEST(PerfHistoryTest, OldDataIsForgotten) {
	PerfHistory history;
	history.Ingest("world:1200:3", WorldReport(), NOW);
	history.Forget(NOW + PerfHistory::RECENT_SECONDS + 10);
	const auto& server = history.Servers().at("world:1200:3");
	EXPECT_EQ(server.seconds.size(), 5u);
	EXPECT_TRUE(server.worst.empty());
	EXPECT_TRUE(server.messages.empty());
	history.Forget(NOW + PerfHistory::SECONDS_KEPT + 10);
	EXPECT_TRUE(history.Servers().at("world:1200:3").seconds.empty());
	history.Forget(NOW + PerfHistory::FORGET_AFTER + 10);
	EXPECT_TRUE(history.Servers().empty());
}

TEST(PerfHistoryTest, ProfilesBecomeAFlameGraphAndFoldedStacks) {
	Profiler::Profile profile;
	profile.frames = 300;
	profile.durationMs = 10000;
	profile.totalUs = 900000;
	profile.nodes = {
		{ .name = "All frames", .depth = 0, .count = 300, .totalUs = 900000 },
		{ .name = Profiler::PACKET, .arg = 77, .depth = 1, .count = 20, .totalUs = 500000 },
		{ .name = "Database query", .depth = 2, .count = 40, .totalUs = 300000 },
		{ .name = "Entities", .depth = 1, .count = 300, .totalUs = 400000 },
	};
	const auto json = PerfHistory::ProfileJson(profile, Label);
	ASSERT_EQ(json["nodes"].size(), 4u);
	EXPECT_EQ(json["nodes"][1]["label"], "Packet named 77");
	EXPECT_EQ(json["nodes"][1]["self_us"], 200000);
	EXPECT_EQ(json["nodes"][0]["self_us"], 0);
	EXPECT_DOUBLE_EQ(json["total_ms"].get<double>(), 900.0);
	EXPECT_EQ(json["folded"],
		"All frames;Packet named 77 200000\n"
		"All frames;Packet named 77;Database query 300000\n"
		"All frames;Entities 400000\n");
}

TEST(PerfHistoryTest, ProfilingIsItsOwnPermission) {
	// Viewing needs health_view; starting a session profiling_run, GM 8 by default and grantable
	const auto* run = Permissions::Find("profiling_run");
	ASSERT_NE(run, nullptr);
	EXPECT_EQ(run->defaultLevel, 8);
	EXPECT_FALSE(run->locked);
	EXPECT_EQ(Permissions::Level("health_view"), 8);
}
