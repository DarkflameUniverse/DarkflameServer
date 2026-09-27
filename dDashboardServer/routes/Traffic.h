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

	// Counters for /metrics
	void AddMetrics(MetricsFormat::Writer& w);
}
