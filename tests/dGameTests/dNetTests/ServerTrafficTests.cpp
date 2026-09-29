#include <gtest/gtest.h>
#include <cstring>

#include "master/ServerTraffic.h"

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
