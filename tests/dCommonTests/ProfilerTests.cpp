#include <gtest/gtest.h>

#include "Profiler.h"

#include <optional>

using namespace Profiler;

namespace {
	constexpr int64_t MS = 1000000; // nanoseconds
	constexpr int64_t UNIX_MS = 1700000000000;

	size_t PhaseIndex(Phase phase) { return static_cast<size_t>(phase); }

	const Node* Find(const std::vector<Node>& nodes, const std::string& name) {
		for (const auto& node : nodes) if (node.name == name) return &node;
		return nullptr;
	}
}

TEST(ProfilerTest, FramesAddUpPerSecond) {
	Recorder recorder;
	int64_t now = 1000 * MS;
	for (int i = 0; i < 3; i++) {
		recorder.FrameBegin(now, UNIX_MS + i * 100);
		recorder.Enter("Entities", 0, now);
		recorder.SetPhase(Phase::ENTITIES, now);
		now += 4 * MS;
		recorder.SetPhase(Phase::OTHER, now);
		recorder.Exit(now);
		now += 1 * MS;
		recorder.FrameEnd(now);
		now += 30 * MS; // asleep
	}
	const auto report = recorder.Take(UNIX_MS / 1000 + 1);
	ASSERT_TRUE(report.present);
	ASSERT_EQ(report.seconds.size(), 1u);
	const auto& second = report.seconds[0];
	EXPECT_EQ(second.time, UNIX_MS / 1000);
	EXPECT_EQ(second.ticks, 3u);
	EXPECT_EQ(second.totalUs, 15000u);
	EXPECT_EQ(second.maxUs, 5000u);
	EXPECT_EQ(second.frames.Count(), 3u);
	EXPECT_EQ(second.phaseUs[PhaseIndex(Phase::ENTITIES)], 12000u);
	EXPECT_EQ(second.phaseUs[PhaseIndex(Phase::OTHER)], 3000u);
	// The longest frames, with their scopes
	ASSERT_EQ(report.worst.size(), Recorder::WORST_FRAMES);
	EXPECT_EQ(report.worst[0].durationUs, 5000u);
	EXPECT_TRUE(report.slow.empty());
}

TEST(ProfilerTest, SecondsMerge) {
	Second a{ .time = 10, .ticks = 2, .totalUs = 3000, .maxUs = 2000 };
	a.frames.Add(1000);
	a.frames.Add(2000);
	a.phaseUs[1] = 500;
	Second b{ .time = 11, .ticks = 1, .totalUs = 9000, .maxUs = 9000 };
	b.frames.Add(9000);
	b.phaseUs[1] = 250;
	a.Merge(b);
	EXPECT_EQ(a.time, 10);
	EXPECT_EQ(a.ticks, 3u);
	EXPECT_EQ(a.totalUs, 12000u);
	EXPECT_EQ(a.maxUs, 9000u);
	EXPECT_EQ(a.frames.Count(), 3u);
	EXPECT_EQ(a.frames.Sum(), 12000u);
	EXPECT_EQ(a.phaseUs[1], 750u);
	// Merged histograms give the percentiles of all their frames
	EXPECT_GE(a.frames.Percentile(1.0), 9000u * 9 / 10);
}

TEST(ProfilerTest, SilentSecondsAreFilledIn) {
	Recorder recorder;
	const int64_t t = UNIX_MS / 1000;
	recorder.FrameBegin(0, UNIX_MS);
	recorder.FrameEnd(1 * MS);
	auto report = recorder.Take(t + 1);
	ASSERT_EQ(report.seconds.size(), 1u);
	// A main loop stuck for 3 seconds: those seconds come as no frames
	report = recorder.Take(t + 4);
	ASSERT_EQ(report.seconds.size(), 3u);
	EXPECT_EQ(report.seconds[0].time, t + 1);
	EXPECT_EQ(report.seconds[2].ticks, 0u);
}

TEST(ProfilerTest, SlowFrameCaptureHasItsScopes) {
	Recorder recorder;
	recorder.SetSlowThreshold(250);
	std::vector<Frame> logged;
	recorder.SetSlowSink([&logged](const Frame& frame) { logged.push_back(frame); });

	int64_t now = 0;
	recorder.FrameBegin(now, UNIX_MS);
	recorder.Enter(PACKET, 42, now);
	recorder.SetPhase(Phase::PACKETS, now);
	now += 1 * MS;
	recorder.Enter("LoadPlayer", 0, now);
	recorder.Enter("CreateEntity", 0, now);
	for (int i = 0; i < 9800; i++) {
		now += MS / 20; // 50 microseconds each
		recorder.Record("CDClient Objects", 0, MS / 20, Phase::CDCLIENT, now);
	}
	recorder.Enter(COMPONENT, 17, now);
	now += 60 * MS;
	recorder.Exit(now);
	recorder.Exit(now); // CreateEntity
	recorder.Exit(now); // LoadPlayer
	recorder.SetPhase(Phase::OTHER, now);
	recorder.Exit(now); // packet
	now += 2 * MS;
	recorder.FrameEnd(now);

	ASSERT_EQ(logged.size(), 1u);
	const auto report = recorder.Take(UNIX_MS / 1000 + 1);
	ASSERT_EQ(report.slow.size(), 1u);
	const auto& frame = report.slow[0];
	EXPECT_EQ(frame.timeMs, UNIX_MS);
	EXPECT_EQ(frame.durationUs, 1000u + 490000u + 60000u + 2000u);
	EXPECT_FALSE(frame.implicit);
	EXPECT_EQ(frame.phaseUs[PhaseIndex(Phase::CDCLIENT)], 490000u);
	EXPECT_EQ(frame.phaseUs[PhaseIndex(Phase::PACKETS)], 61000u);
	EXPECT_EQ(frame.phaseUs[PhaseIndex(Phase::OTHER)], 2000u);

	// The tree, in pre-order with depths, children by when they started
	ASSERT_EQ(frame.scopes.size(), 6u);
	EXPECT_EQ(frame.scopes[0].name, FRAME);
	EXPECT_EQ(frame.scopes[0].depth, 0);
	EXPECT_EQ(frame.scopes[1].name, PACKET);
	EXPECT_EQ(frame.scopes[1].arg, 42u);
	EXPECT_EQ(frame.scopes[2].name, "LoadPlayer");
	EXPECT_EQ(frame.scopes[3].name, "CreateEntity");
	EXPECT_EQ(frame.scopes[3].depth, 3);
	EXPECT_EQ(frame.scopes[4].name, "CDClient Objects");
	EXPECT_EQ(frame.scopes[4].count, 9800u);
	EXPECT_EQ(frame.scopes[4].totalUs, 490000u);
	EXPECT_EQ(frame.scopes[4].depth, 4);
	EXPECT_EQ(frame.scopes[5].name, COMPONENT);
	EXPECT_EQ(frame.scopes[5].totalUs, 60000u);
	EXPECT_EQ(frame.scopes[2].totalUs, 550000u);

	const auto path = frame.Path();
	EXPECT_NE(path.find("LoadPlayer 550.0 ms > CreateEntity 550.0 ms > CDClient Objects 490.0 ms x9800"), std::string::npos) << path;
	// Also in the report's worst frames, cut to fewer scopes
	ASSERT_FALSE(report.worst.empty());
	EXPECT_EQ(report.worst[0].durationUs, frame.durationUs);
}

TEST(ProfilerTest, SlowFramesKeepTheHeaviestScopesWithTheirParents) {
	Recorder recorder;
	recorder.SetSlowThreshold(1);
	int64_t now = 0;
	recorder.FrameBegin(now, UNIX_MS);
	// Many light scopes and one heavy one deep down
	for (int i = 0; i < 60; i++) {
		recorder.Enter(Intern("light " + std::to_string(i)), 0, now);
		now += MS / 100;
		recorder.Exit(now);
	}
	recorder.Enter("a", 0, now);
	recorder.Enter("b", 0, now);
	recorder.Enter("heavy", 0, now);
	now += 10 * MS;
	recorder.Exit(now);
	recorder.Exit(now);
	recorder.Exit(now);
	recorder.FrameEnd(now);
	const auto report = recorder.Take(UNIX_MS / 1000 + 1);
	ASSERT_EQ(report.slow.size(), 1u);
	const auto& scopes = report.slow[0].scopes;
	EXPECT_LE(scopes.size(), Recorder::SLOW_SCOPES + 1);
	const auto* heavy = Find(scopes, "heavy");
	ASSERT_NE(heavy, nullptr);
	EXPECT_EQ(heavy->depth, 3);
	EXPECT_NE(Find(scopes, "a"), nullptr);
	EXPECT_NE(Find(scopes, "b"), nullptr);
}

TEST(ProfilerTest, TooManyDifferentChildrenShareOne) {
	Recorder recorder;
	int64_t now = 0;
	recorder.SetSlowThreshold(1);
	recorder.FrameBegin(now, UNIX_MS);
	for (size_t i = 0; i < Recorder::MAX_CHILDREN + 10; i++) {
		recorder.Enter("child", i + 1, now);
		now += MS / 10;
		recorder.Exit(now);
	}
	now += MS;
	recorder.FrameEnd(now);
	const auto report = recorder.Take(UNIX_MS / 1000 + 1);
	ASSERT_EQ(report.slow.size(), 1u);
	bool more = false;
	for (const auto& node : report.slow[0].scopes) {
		if (node.name == "(more)") {
			more = true;
			EXPECT_EQ(node.count, 10u);
		}
	}
	EXPECT_TRUE(more);
}

TEST(ProfilerTest, WorkOutsideFramesIsItsOwnFrame) {
	Recorder recorder;
	recorder.SetSlowThreshold(100);
	std::vector<Frame> logged;
	recorder.SetSlowSink([&logged](const Frame& frame) { logged.push_back(frame); });
	// A scope with no frame open (a zone load at startup) is timed as one, but not counted as a tick
	recorder.Enter("Zone load", 0, 0);
	EXPECT_TRUE(recorder.InFrame());
	recorder.Exit(400 * MS);
	EXPECT_FALSE(recorder.InFrame());
	ASSERT_EQ(logged.size(), 1u);
	EXPECT_TRUE(logged[0].implicit);
	EXPECT_EQ(logged[0].durationUs, 400000u);
	ASSERT_EQ(logged[0].scopes.size(), 2u);
	EXPECT_EQ(logged[0].scopes[0].name, OUTSIDE);
	EXPECT_EQ(logged[0].scopes[1].name, "Zone load");
	const auto report = recorder.Take(Profiler::UnixMs() / 1000 + 1);
	for (const auto& second : report.seconds) EXPECT_EQ(second.ticks, 0u);
	ASSERT_EQ(report.slow.size(), 1u);
}

TEST(ProfilerTest, SessionsMergeFramesIntoFoldedStacks) {
	Recorder recorder;
	std::optional<Profile> result;
	int64_t now = 0;
	ASSERT_TRUE(recorder.StartSession(5, 1000, now, [&result](Profile&& profile) { result = std::move(profile); }));
	EXPECT_FALSE(recorder.StartSession(6, 1000, now, [](Profile&&) {}));
	for (int i = 0; i < 10; i++) {
		recorder.FrameBegin(now, UNIX_MS);
		recorder.Enter("Entities", 0, now);
		now += 2 * MS;
		recorder.Enter("Script timer", 0, now);
		now += 1 * MS;
		recorder.Exit(now);
		recorder.Exit(now);
		recorder.Enter("Physics step", 0, now);
		now += 1 * MS;
		recorder.Exit(now);
		recorder.FrameEnd(now);
		now += 30 * MS;
	}
	// Not yet: 340 ms of 1000
	EXPECT_FALSE(result.has_value());
	recorder.CheckSession(now + 1000 * MS);
	ASSERT_TRUE(result.has_value());
	EXPECT_FALSE(recorder.SessionActive());
	EXPECT_EQ(result->id, 5u);
	EXPECT_EQ(result->frames, 10u);
	EXPECT_EQ(result->totalUs, 40000u);
	EXPECT_FALSE(result->truncated);
	ASSERT_EQ(result->nodes.size(), 4u);
	EXPECT_EQ(result->nodes[0].name, "All frames");
	EXPECT_EQ(result->nodes[0].count, 10u);
	// Heaviest child first
	EXPECT_EQ(result->nodes[1].name, "Entities");
	EXPECT_EQ(result->nodes[1].count, 10u);
	EXPECT_EQ(result->nodes[1].totalUs, 30000u);
	EXPECT_EQ(result->nodes[2].name, "Script timer");
	EXPECT_EQ(result->nodes[2].depth, 2);
	EXPECT_EQ(result->nodes[3].name, "Physics step");

	// Folded stacks: each stack's own time
	EXPECT_EQ(Folded(result->nodes),
		"All frames;Entities 20000\n"
		"All frames;Entities;Script timer 10000\n"
		"All frames;Physics step 10000\n");
	// With labels (the dashboard names packets); ';' can't appear in a frame name
	EXPECT_EQ(Folded({ { .name = "a;b", .count = 1, .totalUs = 5 } }, [](const Node& node) { return "x" + node.name; }), "xa,b 5\n");
}

TEST(ProfilerTest, SessionsStopEarly) {
	Recorder recorder;
	bool done = false;
	ASSERT_TRUE(recorder.StartSession(1, 60000, 0, [&done](Profile&& profile) { done = true; EXPECT_EQ(profile.frames, 1u); }));
	recorder.FrameBegin(0, UNIX_MS);
	recorder.FrameEnd(MS);
	EXPECT_FALSE(recorder.StopSession(2, MS));
	EXPECT_TRUE(recorder.StopSession(1, MS));
	EXPECT_TRUE(done);
}

TEST(ProfilerTest, MessageTimesAreReportedLongestFirst) {
	Recorder recorder;
	recorder.AddMessageTime(1, 5 * MS);
	recorder.AddMessageTime(2, 1 * MS);
	recorder.AddMessageTime(1, 3 * MS);
	const auto report = recorder.Take(10);
	ASSERT_EQ(report.messages.size(), 2u);
	EXPECT_EQ(report.messages[0].key, 1u);
	EXPECT_EQ(report.messages[0].count, 2u);
	EXPECT_EQ(report.messages[0].totalUs, 8000u);
	EXPECT_EQ(report.messages[0].maxUs, 5000u);
	EXPECT_TRUE(recorder.Take(11).messages.empty());
}

TEST(ProfilerTest, ScopesOffTheMainThreadDoNothing) {
	// This test's thread isn't marked as a main thread: nothing is recorded in the process's recorder
	{
		Scope scope("Worker", Phase::DATABASE);
	}
	EXPECT_FALSE(Local().InFrame());
}
