#include <gtest/gtest.h>
#include <cstring>

#include "master/ServerTraffic.h"
#include "master/Profiling.h"
#include <array>

using namespace TrafficStats;

TEST(ServerTrafficTest, ServerTrafficRoundTrips) {
	ServerTraffic sent;
	sent.serverType = ServiceType::WORLD;
	sent.zoneId = 1100;
	sent.instanceId = 3;
	Second second{ .time = 1700000000, .packetsIn = 5, .packetsOut = 9, .bytesIn = 500, .bytesOut = 9000, .httpRequests = 2 };
	second.httpStatus[1] = 2;
	second.httpLatency.Add(1200);
	second.httpLatency.Add(30000);
	sent.report.seconds = { second, Second{ .time = 1700000001 } };
	sent.report.messages = { MessageCount{ MessageKey{ true, 5, 12, 1234 }, 7, 700 } };
	RouteStats route{ .route = "GET /api/players/:id", .count = 2, .bytesOut = 99 };
	route.status[1] = 2;
	route.latency.Add(800, 2);
	sent.report.routes = { route };
	sent.report.link = { 12, 100, 90, 10000, 9000, 3, 1, 42 };
	sent.report.gauges = { { "workers_busy", 2.0 } };

	RakNet::BitStream stream;
	sent.WritePacket(stream);
	LUBitStream header;
	ASSERT_TRUE(header.ReadHeader(stream));
	EXPECT_EQ(header.internalPacketID, static_cast<uint32_t>(MessageType::Master::SERVER_TRAFFIC));
	ServerTraffic got;
	ASSERT_TRUE(got.Deserialize(stream));
	EXPECT_EQ(got.serverType, ServiceType::WORLD);
	EXPECT_EQ(got.zoneId, 1100u);
	EXPECT_EQ(got.instanceId, 3u);
	ASSERT_EQ(got.report.seconds.size(), 2u);
	EXPECT_EQ(got.report.seconds[0].bytesOut, 9000u);
	EXPECT_EQ(got.report.seconds[0].httpStatus[1], 2u);
	EXPECT_EQ(got.report.seconds[0].httpLatency.Count(), 2u);
	EXPECT_EQ(got.report.seconds[0].httpLatency.Sum(), 31200u);
	ASSERT_EQ(got.report.messages.size(), 1u);
	EXPECT_EQ(got.report.messages[0].key, (MessageKey{ true, 5, 12, 1234 }));
	ASSERT_EQ(got.report.routes.size(), 1u);
	EXPECT_EQ(got.report.routes[0].route, "GET /api/players/:id");
	EXPECT_EQ(got.report.routes[0].latency.Count(), 2u);
	EXPECT_EQ(got.report.link.averagePingMs, 42u);
	EXPECT_EQ(got.report.link.connections, 12u);
	ASSERT_EQ(got.report.gauges.size(), 1u);
	EXPECT_EQ(got.report.gauges[0].first, "workers_busy");

	// Truncated reports are refused
	RakNet::BitStream partial(stream.GetData(), stream.GetNumberOfBytesUsed() - 3, true);
	LUBitStream skip;
	ASSERT_TRUE(skip.ReadHeader(partial));
	ServerTraffic broken;
	EXPECT_FALSE(broken.Deserialize(partial));
}

namespace {
	ServerTraffic Sample(bool split) {
		ServerTraffic sent;
		sent.serverType = ServiceType::AUTH;
		Second a{ .time = 1700000000, .packetsIn = 4, .packetsOut = 3, .bytesIn = 400, .bytesOut = 300 };
		a.peers[0] = { 3, 2, 300, 200 };
		a.peers[1] = { 1, 1, 100, 100 };
		a.httpFromServers = 2;
		a.httpFromServersBytesOut = 64;
		a.httpOutRequests = 1;
		a.httpOutBytesIn = 9;
		sent.report.seconds = { a, Second{ .time = 1700000001 } };
		sent.report.gauges = { { "workers_busy", 1.0 } };
		sent.report.peerSplit = split;
		return sent;
	}

	bool ReadBack(RakNet::BitStream& stream, size_t bytes, ServerTraffic& got) {
		RakNet::BitStream in(stream.GetData(), bytes, true);
		LUBitStream header;
		return header.ReadHeader(in) && got.Deserialize(in);
	}
}

TEST(ServerTrafficTest, PeerSplitRoundTrips) {
	RakNet::BitStream stream;
	Sample(true).WritePacket(stream);
	ServerTraffic got;
	ASSERT_TRUE(ReadBack(stream, stream.GetNumberOfBytesUsed(), got));
	EXPECT_TRUE(got.report.peerSplit);
	ASSERT_EQ(got.report.seconds.size(), 2u);
	EXPECT_EQ(got.report.seconds[0].peers[0], (PeerCounts{ 3, 2, 300, 200 }));
	EXPECT_EQ(got.report.seconds[0].peers[1], (PeerCounts{ 1, 1, 100, 100 }));
	EXPECT_TRUE(got.report.seconds[0].peers[2].Empty());
	EXPECT_EQ(got.report.seconds[0].httpFromServers, 2u);
	EXPECT_EQ(got.report.seconds[0].httpFromServersBytesOut, 64u);
	EXPECT_EQ(got.report.seconds[0].httpOutRequests, 1u);
	EXPECT_EQ(got.report.seconds[0].httpOutBytesIn, 9u);
	EXPECT_TRUE(got.report.seconds[1].peers[0].Empty());
	EXPECT_EQ(got.report.gauges.size(), 1u);

	// A cut-off split is refused
	ServerTraffic broken;
	EXPECT_FALSE(ReadBack(stream, stream.GetNumberOfBytesUsed() - 2, broken));
}

TEST(ServerTrafficTest, ReportsWithoutTheSplitStillRead) {
	// An older server's report is the same bytes without the end: it reads, with no split
	RakNet::BitStream old, now;
	Sample(false).WritePacket(old);
	Sample(true).WritePacket(now);
	ASSERT_LT(old.GetNumberOfBytesUsed(), now.GetNumberOfBytesUsed());
	EXPECT_EQ(0, std::memcmp(old.GetData(), now.GetData(), old.GetNumberOfBytesUsed()));

	ServerTraffic got;
	ASSERT_TRUE(ReadBack(old, old.GetNumberOfBytesUsed(), got));
	EXPECT_FALSE(got.report.peerSplit);
	EXPECT_EQ(got.report.seconds[0].packetsIn, 4u);
	EXPECT_TRUE(got.report.seconds[0].peers[0].Empty());
	EXPECT_EQ(got.report.gauges.size(), 1u);
}

namespace {
	Profiler::Report SampleFrames() {
		Profiler::Report frames;
		frames.present = true;
		frames.slowThresholdMs = 250;
		Profiler::Second second{ .time = 1700000000, .ticks = 30, .totalUs = 90000, .maxUs = 12000 };
		second.frames.Add(3000, 29);
		second.frames.Add(12000);
		second.phaseUs[static_cast<size_t>(Profiler::Phase::PACKETS)] = 40000;
		second.phaseUs[static_cast<size_t>(Profiler::Phase::ENTITIES)] = 30000;
		frames.seconds = { second, Profiler::Second{ .time = 1700000001 } };
		frames.messages = { Profiler::MessageTime{ .key = TrafficStats::MessageKey{ false, 4, 5, 1234 }.Packed(), .count = 3, .totalUs = 700, .maxUs = 400 } };
		Profiler::Frame slow{ .timeMs = 1700000000500, .durationUs = 600000 };
		slow.phaseUs[static_cast<size_t>(Profiler::Phase::CDCLIENT)] = 550000;
		slow.scopes = {
			{ .name = "Frame", .depth = 0, .count = 1, .totalUs = 600000 },
			{ .name = "LoadPlayer", .depth = 1, .count = 1, .totalUs = 590000, .startUs = 100 },
			{ .name = "CDClient Objects", .depth = 2, .count = 9800, .totalUs = 550000, .startUs = 200 },
		};
		frames.slow = { slow };
		frames.worst = { slow };
		return frames;
	}
}

TEST(ServerTrafficTest, FramesRoundTrip) {
	auto sent = Sample(true);
	sent.frames = SampleFrames();
	RakNet::BitStream stream;
	sent.WritePacket(stream);
	ServerTraffic got;
	ASSERT_TRUE(ReadBack(stream, stream.GetNumberOfBytesUsed(), got));
	EXPECT_TRUE(got.report.peerSplit);
	ASSERT_TRUE(got.frames.present);
	EXPECT_EQ(got.frames.slowThresholdMs, 250u);
	ASSERT_EQ(got.frames.seconds.size(), 2u);
	const auto& s = got.frames.seconds[0];
	EXPECT_EQ(s.time, 1700000000);
	EXPECT_EQ(s.ticks, 30u);
	EXPECT_EQ(s.totalUs, 90000u);
	EXPECT_EQ(s.maxUs, 12000u);
	EXPECT_EQ(s.frames.Count(), 30u);
	EXPECT_EQ(s.frames.Sum(), 3000u * 29 + 12000u);
	EXPECT_EQ(s.phaseUs[static_cast<size_t>(Profiler::Phase::PACKETS)], 40000u);
	EXPECT_EQ(s.phaseUs[static_cast<size_t>(Profiler::Phase::ENTITIES)], 30000u);
	EXPECT_EQ(got.frames.seconds[1].ticks, 0u);
	ASSERT_EQ(got.frames.messages.size(), 1u);
	EXPECT_EQ(got.frames.messages[0].count, 3u);
	EXPECT_EQ(got.frames.messages[0].maxUs, 400u);
	ASSERT_EQ(got.frames.slow.size(), 1u);
	ASSERT_EQ(got.frames.worst.size(), 1u);
	const auto& frame = got.frames.slow[0];
	EXPECT_EQ(frame.timeMs, 1700000000500);
	EXPECT_EQ(frame.durationUs, 600000u);
	EXPECT_EQ(frame.phaseUs[static_cast<size_t>(Profiler::Phase::CDCLIENT)], 550000u);
	EXPECT_EQ(frame.scopes, sent.frames.slow[0].scopes);

	// A cut-off section is refused
	ServerTraffic broken;
	EXPECT_FALSE(ReadBack(stream, stream.GetNumberOfBytesUsed() - 2, broken));
}

TEST(ServerTrafficTest, ReportsWithoutFramesStillRead) {
	// An older server's report stops before the frames section: it reads, without frames
	auto withFrames = Sample(true);
	withFrames.frames = SampleFrames();
	RakNet::BitStream old, now;
	Sample(true).WritePacket(old);
	withFrames.WritePacket(now);
	ASSERT_LT(old.GetNumberOfBytesUsed(), now.GetNumberOfBytesUsed());
	EXPECT_EQ(0, std::memcmp(old.GetData(), now.GetData(), old.GetNumberOfBytesUsed()));
	ServerTraffic got;
	ASSERT_TRUE(ReadBack(old, old.GetNumberOfBytesUsed(), got));
	EXPECT_FALSE(got.frames.present);
	EXPECT_TRUE(got.report.peerSplit);
}

TEST(ServerTrafficTest, ReadersStopAtSectionsTheyDontKnow) {
	// A newer server's section after the frames: this reader keeps what it knows and stops there
	auto sent = Sample(true);
	sent.frames = SampleFrames();
	RakNet::BitStream stream;
	sent.WritePacket(stream);
	stream.Write(static_cast<uint8_t>(200));
	stream.Write(static_cast<uint32_t>(0xDEADBEEF));
	ServerTraffic got;
	ASSERT_TRUE(ReadBack(stream, stream.GetNumberOfBytesUsed(), got));
	EXPECT_TRUE(got.frames.present);
	EXPECT_EQ(got.frames.seconds.size(), 2u);
}

TEST(ServerTrafficTest, PhasesOfANewerServerRead) {
	// Phases go with their count: more phases than this reader knows still read, the extra ones skipped
	RakNet::BitStream stream;
	stream.Write(static_cast<uint8_t>(Profiler::PHASES + 2));
	for (uint32_t i = 0; i < Profiler::PHASES + 2; i++) stream.Write(i + 1);
	std::array<uint64_t, Profiler::PHASES> phases{};
	ASSERT_TRUE(ServerTraffic::ReadPhases(stream, phases));
	EXPECT_EQ(phases[0], 1u);
	EXPECT_EQ(phases[Profiler::PHASES - 1], Profiler::PHASES);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST(ServerTrafficTest, ProfileMessagesRoundTrip) {
	ProfileRequest request;
	request.sessionId = 7;
	request.serverType = ServiceType::WORLD;
	request.zoneId = 1200;
	request.instanceId = 2;
	request.durationMs = 10000;
	RakNet::BitStream requestStream;
	request.WritePacket(requestStream);
	LUBitStream header;
	ASSERT_TRUE(header.ReadHeader(requestStream));
	EXPECT_EQ(header.internalPacketID, static_cast<uint32_t>(MessageType::Master::PROFILE_REQUEST));
	ProfileRequest gotRequest;
	ASSERT_TRUE(gotRequest.Deserialize(requestStream));
	EXPECT_EQ(gotRequest.sessionId, 7u);
	EXPECT_EQ(gotRequest.zoneId, 1200u);
	EXPECT_EQ(gotRequest.durationMs, 10000u);
	EXPECT_FALSE(gotRequest.stop);

	ProfileResult result;
	result.sessionId = 7;
	result.serverType = ServiceType::WORLD;
	result.status = eProfileStatus::DONE;
	result.profile.durationMs = 10002;
	result.profile.frames = 300;
	result.profile.totalUs = 450000;
	result.profile.nodes = { { .name = "All frames", .count = 300, .totalUs = 450000 }, { .name = "Entities", .depth = 1, .count = 300, .totalUs = 200000 } };
	RakNet::BitStream resultStream;
	result.WritePacket(resultStream);
	LUBitStream resultHeader;
	ASSERT_TRUE(resultHeader.ReadHeader(resultStream));
	EXPECT_EQ(resultHeader.internalPacketID, static_cast<uint32_t>(MessageType::Master::PROFILE_RESULT));
	ProfileResult got;
	ASSERT_TRUE(got.Deserialize(resultStream));
	EXPECT_EQ(got.status, eProfileStatus::DONE);
	EXPECT_EQ(got.profile.id, 7u);
	EXPECT_EQ(got.profile.frames, 300u);
	EXPECT_EQ(got.profile.nodes, result.profile.nodes);
}
