#include <gtest/gtest.h>

#include <chrono>
#include <cstring>

#include "TrafficStats.h"
#include "MessageIdentifiers.h"
#include "ServiceType.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "MessageType/World.h"

using namespace TrafficStats;

namespace {
	// An LU packet header (and, for game messages, the target object and message ID)
	std::vector<uint8_t> LuPacket(ServiceType service, uint32_t packet, int32_t gameMessage = -1, size_t pad = 0) {
		std::vector<uint8_t> data{ ID_USER_PACKET_ENUM };
		const auto s = static_cast<uint16_t>(service);
		data.push_back(static_cast<uint8_t>(s));
		data.push_back(static_cast<uint8_t>(s >> 8));
		for (int i = 0; i < 4; i++) data.push_back(static_cast<uint8_t>(packet >> (8 * i)));
		data.push_back(0);
		if (gameMessage >= 0) {
			for (int i = 0; i < 8; i++) data.push_back(0x11);
			data.push_back(static_cast<uint8_t>(gameMessage));
			data.push_back(static_cast<uint8_t>(gameMessage >> 8));
		}
		data.resize(data.size() + pad);
		return data;
	}
}

TEST(TrafficStatsTest, HistogramBucketsDoubleEveryThird) {
	EXPECT_EQ(Histogram::UpperBound(0), 100u);
	EXPECT_EQ(Histogram::UpperBound(3), 200u);
	EXPECT_EQ(Histogram::UpperBound(30), 102400u);
	EXPECT_EQ(Histogram::UpperBound(Histogram::BUCKETS - 1), UINT64_MAX);
	EXPECT_EQ(Histogram::BucketFor(0), 0u);
	EXPECT_EQ(Histogram::BucketFor(100), 0u);
	EXPECT_EQ(Histogram::BucketFor(101), 1u);
	EXPECT_EQ(Histogram::BucketFor(200), 3u);
	EXPECT_EQ(Histogram::BucketFor(UINT64_MAX), Histogram::BUCKETS - 1);
	for (size_t i = 1; i + 1 < Histogram::BUCKETS; i++) EXPECT_GT(Histogram::UpperBound(i), Histogram::UpperBound(i - 1));
}

TEST(TrafficStatsTest, PercentilesAreWithinABucket) {
	Histogram h;
	for (uint64_t ms = 1; ms <= 1000; ms++) h.Add(ms * 1000);
	EXPECT_EQ(h.Count(), 1000u);
	EXPECT_EQ(h.Sum(), 500500000u);
	// A bucket spans 26%, so the answer is within that of the exact value
	for (const auto [fraction, exact] : { std::pair{ 0.5, 500000.0 }, std::pair{ 0.95, 950000.0 }, std::pair{ 0.99, 990000.0 } }) {
		const auto p = static_cast<double>(h.Percentile(fraction));
		EXPECT_NEAR(p, exact, exact * 0.26) << fraction;
	}
	EXPECT_LE(h.Percentile(0.5), h.Percentile(0.95));
	EXPECT_LE(h.Percentile(0.95), h.Percentile(0.99));
	EXPECT_EQ(Histogram().Percentile(0.5), 0u);
}

TEST(TrafficStatsTest, SinglePercentileStaysInItsBucket) {
	Histogram h;
	h.Add(150);
	const auto p = h.Percentile(0.99);
	EXPECT_GT(p, Histogram::UpperBound(0));
	EXPECT_LE(p, Histogram::UpperBound(Histogram::BucketFor(150)));
	// Overflow reports its lower bound instead of infinity
	Histogram slow;
	slow.Add(3600ull * 1000000);
	EXPECT_EQ(slow.Percentile(0.5), Histogram::UpperBound(Histogram::BUCKETS - 2));
}

TEST(TrafficStatsTest, HistogramsMergeAndSurviveSparse) {
	Histogram a, b;
	a.Add(500, 3);
	b.Add(500);
	b.Add(40000, 2);
	a.Merge(b);
	EXPECT_EQ(a.Count(), 6u);
	EXPECT_EQ(a.Sum(), 500u * 4 + 80000u);
	const auto sparse = a.Sparse();
	ASSERT_EQ(sparse.size(), 2u);
	const auto back = Histogram::FromSparse(sparse, a.Sum());
	for (size_t i = 0; i < Histogram::BUCKETS; i++) EXPECT_EQ(back.At(i), a.At(i));
	EXPECT_EQ(back.Sum(), a.Sum());
	EXPECT_EQ(back.Percentile(0.5), a.Percentile(0.5));
}

TEST(TrafficStatsTest, StatusClasses) {
	EXPECT_EQ(StatusClass(101), 0u);
	EXPECT_EQ(StatusClass(200), 1u);
	EXPECT_EQ(StatusClass(304), 2u);
	EXPECT_EQ(StatusClass(404), 3u);
	EXPECT_EQ(StatusClass(503), 4u);
	EXPECT_EQ(StatusClass(0), 4u);
	EXPECT_EQ(StatusClass(999), 4u);
}

TEST(TrafficStatsTest, KeysFromPackets) {
	const auto world = LuPacket(ServiceType::WORLD, static_cast<uint32_t>(MessageType::World::POSITION_UPDATE), -1, 20);
	auto key = KeyOf(world.data(), world.size(), false);
	EXPECT_EQ(key.service, static_cast<uint16_t>(ServiceType::WORLD));
	EXPECT_EQ(key.packet, static_cast<uint32_t>(MessageType::World::POSITION_UPDATE));
	EXPECT_EQ(key.gameMessage, 0);
	EXPECT_FALSE(key.outbound);

	const auto gm = LuPacket(ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::GAME_MSG), static_cast<int32_t>(MessageType::Game::REQUEST_USE));
	key = KeyOf(gm.data(), gm.size(), true);
	EXPECT_EQ(key.service, static_cast<uint16_t>(ServiceType::CLIENT));
	EXPECT_EQ(key.gameMessage, static_cast<uint16_t>(MessageType::Game::REQUEST_USE));
	EXPECT_TRUE(key.outbound);

	// A game message cut short has no message ID; never reads past the end
	key = KeyOf(gm.data(), 17, true);
	EXPECT_EQ(key.gameMessage, 0);

	const uint8_t replica[] = { ID_REPLICA_MANAGER_SERIALIZE, 1, 2 };
	key = KeyOf(replica, sizeof(replica), true);
	EXPECT_EQ(key.service, MessageKey::RAKNET);
	EXPECT_EQ(key.packet, static_cast<uint32_t>(ID_REPLICA_MANAGER_SERIALIZE));

	const uint8_t shortLu[] = { ID_USER_PACKET_ENUM, 4, 0 };
	EXPECT_EQ(KeyOf(shortLu, sizeof(shortLu), false).service, MessageKey::RAKNET);
	EXPECT_EQ(KeyOf(nullptr, 0, false).service, MessageKey::RAKNET);
}

TEST(TrafficStatsTest, KeysPackAndUnpack) {
	for (const auto& key : { MessageKey{ true, 5, 12, 1234 }, MessageKey{ false, MessageKey::RAKNET, 36, 0 }, MessageKey{ false, 4, 0xFFFFFFFF, 0xFFFF } }) {
		EXPECT_EQ(MessageKey::Unpack(key.Packed()), key);
	}
	EXPECT_NE((MessageKey{ true, 5, 12, 0 }).Packed(), (MessageKey{ false, 5, 12, 0 }).Packed());
}

TEST(TrafficStatsTest, RecorderFillsSilentSecondsAndKeepsTheCurrentOne) {
	Recorder r;
	const MessageKey in{ false, 4, 5, 0 };
	const MessageKey out{ true, 5, 12, 0 };
	r.Packet(1000, in, 100);
	r.Packet(1000, in, 50);
	r.Packet(1000, out, 30, 4); // a broadcast to four
	r.Packet(1003, in, 10);
	r.Packet(1005, in, 10); // the current second: stays
	const auto report = r.Take(1005);
	ASSERT_EQ(report.seconds.size(), 5u); // 1000..1004
	EXPECT_EQ(report.seconds[0].time, 1000);
	EXPECT_EQ(report.seconds[0].packetsIn, 2u);
	EXPECT_EQ(report.seconds[0].bytesIn, 150u);
	EXPECT_EQ(report.seconds[0].packetsOut, 4u);
	EXPECT_EQ(report.seconds[0].bytesOut, 120u);
	EXPECT_TRUE(report.seconds[1].Idle());
	EXPECT_EQ(report.seconds[3].packetsIn, 1u);
	EXPECT_EQ(report.seconds[4].time, 1004);

	const auto next = r.Take(1008);
	ASSERT_EQ(next.seconds.size(), 3u); // 1005..1007, no second twice
	EXPECT_EQ(next.seconds[0].time, 1005);
	EXPECT_EQ(next.seconds[0].packetsIn, 1u);
}

TEST(TrafficStatsTest, RecorderCapsLongSilences) {
	Recorder r;
	r.Packet(1000, MessageKey{}, 1);
	r.Take(1001);
	const auto report = r.Take(1001 + 10000);
	EXPECT_EQ(report.seconds.size(), static_cast<size_t>(Recorder::MAX_GAP));
	EXPECT_EQ(report.seconds.back().time, 1000 + 10000);
}

TEST(TrafficStatsTest, RecorderTopMessagesPerDirection) {
	Recorder r;
	for (uint32_t id = 0; id < 40; id++) {
		for (uint32_t n = 0; n <= id; n++) {
			r.Packet(1, MessageKey{ false, 4, id, 0 }, 10);
			r.Packet(1, MessageKey{ true, 5, id, 0 }, 10);
		}
	}
	const auto report = r.Take(2);
	ASSERT_EQ(report.messages.size(), Recorder::TOP_MESSAGES * 2);
	EXPECT_FALSE(report.messages.front().key.outbound);
	EXPECT_EQ(report.messages.front().key.packet, 39u);
	EXPECT_EQ(report.messages.front().count, 40u);
	EXPECT_TRUE(report.messages[Recorder::TOP_MESSAGES].key.outbound);
	EXPECT_TRUE(r.Take(3).messages.empty());
}

TEST(TrafficStatsTest, RecorderHttpAndRoutes) {
	Recorder r;
	r.Http(10, "GET /api/players", 200, 1500, 2000);
	r.Http(10, "GET /api/players", 404, 300, 20);
	r.Http(11, "POST /api/login", 500, 90000, 50);
	for (size_t i = 0; i < Recorder::MAX_ROUTES + 5; i++) r.Http(11, "GET /r" + std::to_string(i), 200, 100, 1);
	r.SetGauge("workers_busy", [] { return 3.0; });
	const auto report = r.Take(12);
	ASSERT_EQ(report.seconds.size(), 2u);
	EXPECT_EQ(report.seconds[0].httpRequests, 2u);
	EXPECT_EQ(report.seconds[0].httpStatus[1], 1u);
	EXPECT_EQ(report.seconds[0].httpStatus[3], 1u);
	EXPECT_EQ(report.seconds[0].httpBytesOut, 2020u);
	EXPECT_EQ(report.seconds[0].httpLatency.Count(), 2u);
	EXPECT_EQ(report.routes.size(), Recorder::MAX_ROUTES + 1); // the rest counted as "other"
	const auto other = std::find_if(report.routes.begin(), report.routes.end(), [](const RouteStats& s) { return s.route == "other"; });
	ASSERT_NE(other, report.routes.end());
	EXPECT_EQ(other->count, 7u);
	ASSERT_EQ(report.gauges.size(), 1u);
	EXPECT_EQ(report.gauges[0].second, 3.0);
}

TEST(TrafficStatsTest, DueAfterTheInterval) {
	Recorder r;
	EXPECT_FALSE(r.Due(100, 5)); // starts the clock
	EXPECT_FALSE(r.Due(104, 5));
	EXPECT_TRUE(r.Due(105, 5));
	r.Take(105);
	EXPECT_FALSE(r.Due(106, 5));
}

// Counting is on every packet's path: keep it cheap
TEST(TrafficStatsTest, CountingIsCheap) {
	Recorder r;
	const auto packet = LuPacket(ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::GAME_MSG), 100, 30);
	constexpr int N = 1000000;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < N; i++) r.Packet(Now(), KeyOf(packet.data(), packet.size(), (i & 1) != 0), packet.size());
	const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count() / N;
	std::printf("[          ] KeyOf + Recorder::Packet: %lld ns per packet\n", static_cast<long long>(ns));
	EXPECT_LT(ns, 2000); // generous for slow CI machines and sanitizers
}

TEST(TrafficStatsTest, RecorderSplitsPacketsByPeer) {
	Recorder r;
	const MessageKey in{ false, 4, 5, 0 };
	const MessageKey out{ true, 5, 12, 0 };
	r.Packet(2000, in, 100); // clients by default
	r.Packet(2000, out, 40, 3, Peer::CLIENTS);
	r.Packet(2000, out, 20, 1, Peer::MASTER);
	r.Packet(2000, in, 60, 1, Peer::MASTER);
	r.Packet(2000, in, 8, 1, Peer::SERVERS);
	const auto report = r.Take(2001);
	EXPECT_TRUE(report.peerSplit);
	ASSERT_EQ(report.seconds.size(), 1u);
	const auto& s = report.seconds[0];
	EXPECT_EQ(s.peers[0], (PeerCounts{ 1, 3, 100, 120 }));
	EXPECT_EQ(s.peers[1], (PeerCounts{ 1, 1, 60, 20 }));
	EXPECT_EQ(s.peers[2], (PeerCounts{ 1, 0, 8, 0 }));
	// The split adds up to the totals
	uint64_t packetsIn = 0, bytesOut = 0;
	for (const auto& p : s.peers) { packetsIn += p.packetsIn; bytesOut += p.bytesOut; }
	EXPECT_EQ(packetsIn, s.packetsIn);
	EXPECT_EQ(bytesOut, s.bytesOut);

	Second merged = s;
	merged.Merge(s);
	EXPECT_EQ(merged.peers[1], (PeerCounts{ 2, 2, 120, 40 }));
}

TEST(TrafficStatsTest, RecorderSplitsHttpByWhoAsked) {
	Recorder r;
	r.Http(3000, "GET /api/a", 200, 100, 1000);
	r.Http(3000, "GET /api/a", 200, 100, 500, true);
	r.HttpOut(3000, 700);
	const auto report = r.Take(3001);
	ASSERT_EQ(report.seconds.size(), 1u);
	EXPECT_EQ(report.seconds[0].httpRequests, 2u);
	EXPECT_EQ(report.seconds[0].httpFromServers, 1u);
	EXPECT_EQ(report.seconds[0].httpFromServersBytesOut, 500u);
	EXPECT_EQ(report.seconds[0].httpOutRequests, 1u);
	EXPECT_EQ(report.seconds[0].httpOutBytesIn, 700u);
}

TEST(TrafficStatsTest, HttpClientsApartBySignedInAccount) {
	Recorder r;
	r.HttpClient("203.0.113.5", false, 100, 1000, 7, "alice");
	r.HttpClient("203.0.113.5", false, 100, 1000, 7, "alice");
	r.HttpClient("203.0.113.5", false, 50, 500, 9, "bob");
	r.HttpClient("203.0.113.5", false, 10, 20); // not signed in (the sign-in page)
	const auto report = r.Take(1);
	ASSERT_EQ(report.connections.size(), 3u);
	for (const auto& c : report.connections) {
		EXPECT_EQ(c.address, "203.0.113.5");
		EXPECT_TRUE(c.http);
		if (c.accountId == 7) {
			EXPECT_EQ(c.account, "alice");
			EXPECT_EQ(c.packetsIn, 2u);
			EXPECT_EQ(c.bytesOut, 2000u);
		} else if (c.accountId == 9) {
			EXPECT_EQ(c.account, "bob");
		} else {
			EXPECT_EQ(c.accountId, 0u);
			EXPECT_TRUE(c.account.empty());
			EXPECT_EQ(c.bytesIn, 10u);
		}
	}
}
