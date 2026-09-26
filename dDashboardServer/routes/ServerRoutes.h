#pragma once

/**
 * Running the server: health history (a sample a minute: players, worlds, auth/chat up, memory), crash dumps from
 * dump_folder, and searching the servers' log files. Prometheus metrics are in PrometheusMetrics.h.
 */
void RegisterServerRoutes();

namespace ServerRoutes {
	// Main loop: take a health sample once a minute
	void Update();
}
