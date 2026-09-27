#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "IServerTraffic.h"
#include "TrafficStats.h"

/**
 * The traffic reports of every server, kept by the dashboard: the last hour at one second, the minutes that are not in
 * the database yet, the busiest message types of the last hour and day, and totals since the dashboard started (for
 * Prometheus). Pure (no database, network or clock), so it is unit tested; Traffic.cpp feeds it and writes the minutes.
 */
class TrafficHistory {
public:
	static constexpr int64_t SECONDS_KEPT = 3600;
	static constexpr int64_t MINUTE_SLACK = 20;          // seconds a minute waits for late reports before it is final
	static constexpr int64_t FORGET_AFTER = 86400;       // a server silent this long is dropped
	static constexpr size_t MESSAGE_MINUTES = 60;         // per-minute message counts kept
	static constexpr size_t MESSAGE_HOURS = 24;           // per-hour message counts kept
	static constexpr size_t MAX_TOTAL_MESSAGES = 256;     // message types a server's totals keep
	static constexpr size_t MAX_TOTAL_ROUTES = 128;       // routes a server's totals keep
	static constexpr size_t ROUTE_MINUTES = 60;           // per-minute route stats kept

	// One second (or a bucket of them) of one server
	struct Point {
		int64_t time{};
		uint64_t packetsIn{}, packetsOut{}, bytesIn{}, bytesOut{};
		uint64_t httpRequests{}, http4xx{}, http5xx{}, httpBytesOut{};
		std::vector<std::pair<uint8_t, uint32_t>> latency; // sparse HTTP latency histogram, empty without requests
		uint64_t latencySum{};

		void Add(const TrafficStats::Second& second);
		void Add(const Point& other);
		TrafficStats::Histogram Latency() const;
	};

	struct Minute {
		Point point;
		uint64_t resends{};
	};

	using MessageCounts = std::map<uint64_t, TrafficStats::MessageCount>; // by MessageKey::Packed

	struct Totals {
		uint64_t packetsIn{}, packetsOut{}, bytesIn{}, bytesOut{};
		uint64_t datagramsSent{}, datagramsReceived{}, linkBytesSent{}, linkBytesReceived{}, resends{};
		MessageCounts messages;
		std::map<std::string, TrafficStats::RouteStats> routes;
	};

	struct Server {
		std::string key;
		uint16_t type{}; // ServiceType
		uint32_t zoneId{};
		uint32_t instanceId{};
		int64_t firstSeen{};
		int64_t lastSeen{};
		std::deque<Point> seconds;  // oldest first, up to SECONDS_KEPT
		std::deque<Minute> minutes; // not written yet
		int64_t writtenUntil{};     // minutes before this were handed out by TakeFinishedMinutes
		TrafficStats::Link link;    // the last report's
		std::vector<std::pair<std::string, double>> gauges;
		Totals totals;
		std::deque<std::pair<int64_t, MessageCounts>> messageMinutes;
		std::deque<std::pair<int64_t, MessageCounts>> messageHours;
		std::deque<std::pair<int64_t, std::map<std::string, TrafficStats::RouteStats>>> routeMinutes;
	};

	// "master", "auth", "chat", "dashboard", "ugc", "world:<zone>:<instance>"
	static std::string KeyFor(uint16_t serviceType, uint32_t zoneId, uint32_t instanceId);

	void Ingest(uint16_t serviceType, uint32_t zoneId, uint32_t instanceId, const TrafficStats::Report& report, int64_t now);

	// Minutes that are final (and not handed out before), as database rows
	std::vector<IServerTraffic::TrafficMinute> TakeFinishedMinutes(int64_t now);

	// Drop servers silent for FORGET_AFTER and seconds older than SECONDS_KEPT
	void Forget(int64_t now);

	const std::map<std::string, Server>& Servers() const { return m_Servers; }

	/**
	 * The seconds from `from` (inclusive) to `to` (exclusive) summed into buckets of `step` seconds, per server:
	 * index i is the bucket starting at from + i * step. Servers with nothing in the range are left out.
	 */
	std::map<std::string, std::vector<Point>> Buckets(int64_t from, int64_t to, int64_t step) const;

	// The busiest message types since `since` (minute counts for the last hour, hour counts before), one server or all ("")
	std::vector<TrafficStats::MessageCount> TopMessages(const std::string& server, int64_t since, size_t limit) const;

	// The HTTP routes since `since` (the last hour at most) of every server with a web server, as (server, route); busiest first
	std::vector<std::pair<std::string, TrafficStats::RouteStats>> Routes(int64_t since) const;

private:
	std::map<std::string, Server> m_Servers;
};
