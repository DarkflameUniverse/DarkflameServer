#pragma once

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

	// Counters for /metrics
	void AddMetrics(MetricsFormat::Writer& w);
}
