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

TEST(NetworkViewTest, ConnectionsGroupByPlayerOrConnectionAndHideIt) {
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
	// The player's game connection, the same address fetching from the UGC server, and the master link are three
	// entries: an address shared by a player's browser, other players or the servers' own links is not one peer
	ASSERT_EQ(shown["peers"].size(), 3u);
	const auto find = [&](const std::string& kind) -> const nlohmann::json& {
		for (const auto& p : shown["peers"]) if (p["kind"] == kind) return p;
		return shown["peers"][0];
	};
	const auto& gamePeer = find("game");
	EXPECT_EQ(gamePeer["kind"], "game");
	EXPECT_EQ(gamePeer["address"], "203.0.113.5");
	EXPECT_EQ(gamePeer["account"], "alice");
	EXPECT_EQ(gamePeer["character"], "Alice");
	EXPECT_EQ(gamePeer["character_id"], "1152921504606846976");
	ASSERT_EQ(gamePeer["servers"].size(), 1u);
	EXPECT_EQ(find("web")["kind"], "web");
	EXPECT_EQ(find("server")["kind"], "server");
	ASSERT_EQ(shown["others"].size(), 1u);
	EXPECT_EQ(shown["others"][0]["count"], 4);

	// Without network_ips: a token instead of the address, the same for the same address, no ports
	const auto hidden = NetworkView::Connections(history, NOW, 20, false, 1234, Label);
	const auto dump = hidden.dump();
	EXPECT_EQ(dump.find("203.0.113.5"), std::string::npos);
	EXPECT_EQ(dump.find("127.0.0.1"), std::string::npos);
	for (const auto& p : hidden["peers"]) if (p["kind"] == "game") EXPECT_EQ(p["address"], NetworkView::MaskAddress("203.0.113.5", 1234));
	for (const auto& entry : hidden["peers"][0]["servers"]) EXPECT_TRUE(!entry.contains("port") || entry["port"].is_null());
	EXPECT_NE(NetworkView::MaskAddress("203.0.113.5", 1234), NetworkView::MaskAddress("203.0.113.5", 99));
	EXPECT_NE(NetworkView::MaskAddress("203.0.113.5", 1234), NetworkView::MaskAddress("203.0.113.6", 1234));

	// The summary never carries addresses
	EXPECT_EQ(NetworkView::Summary(history, NOW, 20, Label).dump().find("203.0.113.5"), std::string::npos);
}

TEST(NetworkViewTest, ServerLinksOnThePlayersAddressStayApart) {
	// Everything on one machine: a player and the chat server's links from the worlds all come from 127.0.0.1
	TrafficHistory history;
	auto world = WorldReport(true);
	world.connections = { Connection{ .address = "127.0.0.1", .port = 50001, .peer = Peer::CLIENTS, .bytesIn = 900, .bytesOut = 900, .accountId = 7, .account = "alice" } };
	world.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 3, world, NOW);
	auto chat = WorldReport(true);
	chat.connections = { Connection{ .address = "127.0.0.1", .port = 50100, .peer = Peer::SERVERS, .bytesIn = 50 },
		Connection{ .address = "127.0.0.1", .port = 50101, .peer = Peer::SERVERS, .bytesIn = 50 } };
	chat.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::CHAT), 0, 0, chat, NOW);

	const auto json = NetworkView::Connections(history, NOW, 20, true, 1, Label);
	ASSERT_EQ(json["peers"].size(), 3u);
	for (const auto& p : json["peers"]) {
		if (p["kind"] == "game") {
			EXPECT_EQ(p["account"], "alice");
			ASSERT_EQ(p["servers"].size(), 1u); // only the world, not the chat server's links
		} else {
			EXPECT_EQ(p["kind"], "server");
			EXPECT_FALSE(p.contains("account"));
		}
	}
}

TEST(NetworkViewTest, WebClientsBySignedInAccountWithTheirUserName) {
	// Two staff members behind one address and a request that wasn't signed in, on the dashboard
	TrafficHistory history;
	Report dashboard;
	dashboard.seconds = { Second{ .time = NOW - 1 } };
	dashboard.connections = {
		Connection{ .address = "198.51.100.7", .peer = Peer::CLIENTS, .http = true, .packetsIn = 4, .packetsOut = 4, .bytesIn = 400, .bytesOut = 40000, .accountId = 7, .account = "alice" },
		Connection{ .address = "198.51.100.7", .peer = Peer::CLIENTS, .http = true, .packetsIn = 2, .packetsOut = 2, .bytesIn = 200, .bytesOut = 20000, .accountId = 9, .account = "bob" },
		Connection{ .address = "198.51.100.7", .peer = Peer::CLIENTS, .http = true, .packetsIn = 1, .packetsOut = 1, .bytesIn = 100, .bytesOut = 100 } };
	dashboard.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::DASHBOARD), 0, 0, dashboard, NOW);

	const auto shown = NetworkView::Connections(history, NOW, 20, true, 5, Label);
	ASSERT_EQ(shown["peers"].size(), 3u);
	std::vector<std::string> users;
	for (const auto& p : shown["peers"]) {
		EXPECT_EQ(p["kind"], "web");
		EXPECT_EQ(p["address"], "198.51.100.7");
		EXPECT_FALSE(p.contains("account")); // a web client's name is its dashboard user, not a player
		users.push_back(p.value("user", std::string("-")));
	}
	EXPECT_EQ(users, (std::vector<std::string>{ "alice", "bob", "-" })); // busiest first
	EXPECT_EQ(shown["peers"][0]["account_id"], 7);

	// Without network_ips: still the user names, and the address as its token (the same for all three)
	const auto hidden = NetworkView::Connections(history, NOW, 20, false, 5, Label);
	EXPECT_EQ(hidden.dump().find("198.51.100.7"), std::string::npos);
	ASSERT_EQ(hidden["peers"].size(), 3u);
	EXPECT_EQ(hidden["peers"][0]["user"], "alice");
	EXPECT_EQ(hidden["peers"][1]["user"], "bob");
	for (const auto& p : hidden["peers"]) EXPECT_EQ(p["address"], NetworkView::MaskAddress("198.51.100.7", 5));
}

TEST(NetworkViewTest, ListeningPortsAndMachinesFromTheServerList) {
	// Auth and a world on master's machine, a second world on another; chat is reporting but not in the list yet
	TrafficHistory history;
	history.Ingest(static_cast<uint16_t>(ServiceType::AUTH), 0, 0, WorldReport(true), NOW);
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 3, WorldReport(true), NOW);
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 4, WorldReport(true), NOW);
	history.Ingest(static_cast<uint16_t>(ServiceType::CHAT), 0, 0, WorldReport(true), NOW);
	const NetworkView::Endpoints endpoints{ { "auth", { 1001, "192.0.2.1" } }, { "world:1200:3", { 3015, "192.0.2.1" } },
		{ "world:1200:4", { 3016, "198.51.100.20" } }, { "master", { 2000, "192.0.2.1" } } };

	const auto json = NetworkView::Summary(history, NOW, 20, Label, endpoints, 77);
	const auto& servers = json["servers"];
	EXPECT_EQ(servers["auth"]["port"], 1001);
	EXPECT_EQ(servers["world:1200:3"]["port"], 3015);
	EXPECT_EQ(servers["world:1200:4"]["port"], 3016);
	// The machine is a token, the same for servers on the same machine
	EXPECT_EQ(servers["auth"]["host"], NetworkView::MaskAddress("192.0.2.1", 77));
	EXPECT_EQ(servers["auth"]["host"], servers["world:1200:3"]["host"]);
	EXPECT_EQ(servers["world:1200:4"]["host"], NetworkView::MaskAddress("198.51.100.20", 77));
	EXPECT_NE(servers["auth"]["host"], servers["world:1200:4"]["host"]);
	// Not in the list: no port or machine
	EXPECT_TRUE(servers["chat"]["port"].is_null());
	EXPECT_TRUE(servers["chat"]["host"].is_null());
	// It goes to everyone: no addresses
	const auto dump = json.dump();
	EXPECT_EQ(dump.find("192.0.2.1"), std::string::npos);
	EXPECT_EQ(dump.find("198.51.100.20"), std::string::npos);

	// The connection list names the machines only with network_ips
	const auto shown = NetworkView::Connections(history, NOW, 20, true, 77, Label, endpoints);
	ASSERT_EQ(shown["hosts"].size(), 2u);
	EXPECT_EQ(shown["hosts"][NetworkView::MaskAddress("192.0.2.1", 77)], "192.0.2.1");
	EXPECT_EQ(shown["hosts"][NetworkView::MaskAddress("198.51.100.20", 77)], "198.51.100.20");
	const auto hidden = NetworkView::Connections(history, NOW, 20, false, 77, Label, endpoints);
	EXPECT_TRUE(hidden["hosts"].empty());
	EXPECT_EQ(hidden.dump().find("192.0.2.1"), std::string::npos);
}

TEST(NetworkViewTest, RemotePortsOnlyWithAddresses) {
	// A player's and a server link's remote port: shown with network_ips, left out without
	TrafficHistory history;
	auto world = WorldReport(true);
	world.connections = { Connection{ .address = "203.0.113.9", .port = 51234, .peer = Peer::CLIENTS, .bytesIn = 10, .accountId = 3, .account = "carol" },
		Connection{ .address = "192.0.2.1", .port = 2000, .peer = Peer::MASTER, .bytesIn = 10 } };
	world.hasConnections = true;
	history.Ingest(static_cast<uint16_t>(ServiceType::WORLD), 1200, 3, world, NOW);
	for (const bool show : { true, false }) {
		const auto json = NetworkView::Connections(history, NOW, 20, show, 1, Label);
		ASSERT_EQ(json["peers"].size(), 2u);
		for (const auto& p : json["peers"]) {
			const auto& port = p["servers"][0]["port"];
			if (!show) EXPECT_TRUE(port.is_null());
			else EXPECT_EQ(port, p["kind"] == "game" ? 51234 : 2000);
		}
	}
}
