#pragma once

#include <string>
#include "json.hpp"

struct ServerTraffic;
namespace MetricsFormat { class Writer; }

/**
 * Traffic diagnostics: every server's packets, bytes and HTTP requests (see ServerTraffic.h and TrafficHistory.h).
 * The last hour is kept in memory at one second; finished minutes go to the server_traffic table once a minute.
 */
namespace Traffic {
	void RegisterRoutes();

	// SERVER_TRAFFIC from a server (via master), or the dashboard's own report
	void Ingest(const ServerTraffic& report);

	// Main loop: write finished minutes, tell open pages there is news
	void Update();

	// Main thread: one server's last report ("ugc", "chat", ...): {key, label, online, last_seen, link, gauges}
	nlohmann::json Server(const std::string& key);

	// "World 1200 Nimbus Station #3" for world:1200:3, "Master" for master, ...
	std::string Label(const std::string& key);

	// A packet type's names (MessageKey::Packed): {service, packet, game_message}
	nlohmann::json MessageNames(uint64_t packedKey);

	// Counters for /metrics
	void AddMetrics(MetricsFormat::Writer& w);
}
