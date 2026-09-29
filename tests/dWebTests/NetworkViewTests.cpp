#include <gtest/gtest.h>

#include "NetworkView.h"
#include "ServiceType.h"

using namespace TrafficStats;

namespace {
	constexpr int64_t NOW = 1700000100;

	// Five seconds of a world: 10 packets in and 20 out a second, split between its players and master
	Report WorldReport(bool split) {
		Report report;
		for (int64_t t = NOW - 5; t < NOW; t++) {
			Second s{ .time = t, .packetsIn = 10, .packetsOut = 20, .bytesIn = 1000, .bytesOut = 4000 };
			s.peers[static_cast<size_t>(Peer::CLIENTS)] = { 8, 18, 800, 3800 };
			s.peers[static_cast<size_t>(Peer::MASTER)] = { 1, 1, 100, 100 };
			s.peers[static_cast<size_t>(Peer::SERVERS)] = { 1, 1, 100, 100 };
			report.seconds.push_back(s);
		}
		report.peerSplit = split;
		report.link = { .connections = 3, .resends = 2, .averagePingMs = 40 };
		return report;
	}

	std::string Label(const TrafficHistory::Server& server) { return "label of " + server.key; }
}

TEST(NetworkViewTest, SummaryHasRatesAndTheSplit) {
	TrafficHistory history;
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 3, WorldReport(true), NOW);
	const auto json = NetworkView::Summary(history, NOW, 20, Label);
	ASSERT_TRUE(json["servers"].contains("world:1200:3"));
	const auto& world = json["servers"]["world:1200:3"];
	EXPECT_EQ(world["label"], "label of world:1200:3");
	EXPECT_EQ(world["type"], "WORLD");
	EXPECT_EQ(world["zone"], 1200);
	EXPECT_DOUBLE_EQ(world["packets_in"].get<double>(), 10.0);
	EXPECT_DOUBLE_EQ(world["bytes_out"].get<double>(), 4000.0);
	EXPECT_EQ(world["link"]["connections"], 3);
	EXPECT_EQ(world["link"]["ping_ms"], 40);
	ASSERT_TRUE(world["split"].is_object());
	EXPECT_DOUBLE_EQ(world["split"]["clients"]["packets_out"].get<double>(), 18.0);
	EXPECT_DOUBLE_EQ(world["split"]["master"]["bytes_in"].get<double>(), 100.0);
	EXPECT_DOUBLE_EQ(world["split"]["servers"]["packets_in"].get<double>(), 1.0);
	EXPECT_EQ(json["window"], NetworkView::WINDOW);
}

TEST(NetworkViewTest, OlderServersHaveNoSplit) {
	TrafficHistory history;
	history.Ingest(static_cast<uint16_t>(ServiceType::AUTH), 0, 0, WorldReport(false), NOW);
	const auto json = NetworkView::Summary(history, NOW, 20, Label);
	ASSERT_TRUE(json["servers"].contains("auth"));
	EXPECT_TRUE(json["servers"]["auth"]["split"].is_null());
	EXPECT_DOUBLE_EQ(json["servers"]["auth"]["packets_in"].get<double>(), 10.0);

	// Silent servers are left out
	EXPECT_TRUE(NetworkView::Summary(history, NOW + 60, 20, Label)["servers"].empty());
}

TEST(NetworkViewTest, HttpSplitOfTheUgcServer) {
	// The UGC server: all its requests came from the dashboard (the wire format is tested in ServerTrafficTests)
	auto report = WorldReport(true);
	report.seconds[0].httpRequests = 5;
	report.seconds[0].httpFromServers = 5;
	TrafficHistory history;
	history.Ingest(static_cast<uint16_t>(ServiceType::UGC), 0, 0, report, NOW);
	const auto json = NetworkView::Summary(history, NOW, 20, nullptr);
	const auto& ugc = json["servers"]["ugc"];
	EXPECT_EQ(ugc["label"], "ugc");
	EXPECT_DOUBLE_EQ(ugc["http"].get<double>(), 1.0);
	EXPECT_DOUBLE_EQ(ugc["split"]["http_from_servers"].get<double>(), 1.0);
}

TEST(NetworkViewTest, ConnectionsGroupByAddressAndHideIt) {
	TrafficHistory history;
	auto world = WorldReport(true);
	Connection player{ .address = "203.0.113.5", .port = 50000, .peer = Peer::CLIENTS, .packetsIn = 50, .packetsOut = 100, .bytesIn = 5000, .bytesOut = 10000,
		.resends = 1, .pingMs = 45, .accountId = 7, .characterId = 1152921504606846976ull, .account = "alice", .character = "Alice" };
	Connection master{ .address = "127.0.0.1", .port = 2000, .peer = Peer::MASTER, .bytesIn = 10, .bytesOut = 10 };
	world.connections = { player, master };
	world.otherConnections = Connection{ .bytesIn = 500 };
	world.otherConnectionCount = 4;
	world.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 3, world, NOW);

	Report ugc;
	ugc.seconds = { Second{ .time = NOW - 1 } };
	ugc.connections = { Connection{ .address = "203.0.113.5", .peer = Peer::CLIENTS, .http = true, .packetsIn = 2, .packetsOut = 2, .bytesIn = 300, .bytesOut = 90000 } };
	ugc.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::UGC), 0, 0, ugc, NOW);

	const auto shown = NetworkView::Connections(history, NOW, 20, true, 1234, Label);
	EXPECT_TRUE(shown["addresses_shown"].get<bool>());
	ASSERT_EQ(shown["peers"].size(), 2u);
	const auto& first = shown["peers"][0]; // the busiest: the player on the world and the UGC server
	EXPECT_EQ(first["address"], "203.0.113.5");
	EXPECT_EQ(first["kind"], "game");
	EXPECT_EQ(first["account"], "alice");
	EXPECT_EQ(first["character"], "Alice");
	EXPECT_EQ(first["character_id"], "1152921504606846976");
	ASSERT_EQ(first["servers"].size(), 2u);
	EXPECT_EQ(shown["peers"][1]["kind"], "server");
	ASSERT_EQ(shown["others"].size(), 1u);
	EXPECT_EQ(shown["others"][0]["count"], 4);

	// Without network_ips: a token instead of the address, the same for the same address, no ports
	const auto hidden = NetworkView::Connections(history, NOW, 20, false, 1234, Label);
	const auto dump = hidden.dump();
	EXPECT_EQ(dump.find("203.0.113.5"), std::string::npos);
	EXPECT_EQ(dump.find("127.0.0.1"), std::string::npos);
	EXPECT_EQ(hidden["peers"][0]["address"], NetworkView::MaskAddress("203.0.113.5", 1234));
	for (const auto& entry : hidden["peers"][0]["servers"]) EXPECT_TRUE(!entry.contains("port") || entry["port"].is_null());
	EXPECT_NE(NetworkView::MaskAddress("203.0.113.5", 1234), NetworkView::MaskAddress("203.0.113.5", 99));
	EXPECT_NE(NetworkView::MaskAddress("203.0.113.5", 1234), NetworkView::MaskAddress("203.0.113.6", 1234));

	// The summary never carries addresses
	EXPECT_EQ(NetworkView::Summary(history, NOW, 20, Label).dump().find("203.0.113.5"), std::string::npos);
}
