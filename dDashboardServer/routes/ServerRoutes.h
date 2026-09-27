#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * Running the server: health history (a sample a minute: players, worlds, auth/chat/UGC up, memory), the running
 * server processes, crash dumps from dump_folder, and searching the servers' log files. Prometheus metrics are in
 * PrometheusMetrics.h.
 */
void RegisterServerRoutes();

namespace ServerRoutes {
	// Main loop: take a health sample once a minute
	void Update();

	// A running DarkflameServer process from this build
	struct Process {
		std::string program; // the binary: "WorldServer", "UgcServer", ...
		uint32_t pid{};
		uint64_t memoryKb{};  // resident
		double cpuPercent{};  // of one core, since this process was last looked at (0 the first time)
		int64_t startedAt{};  // unix time
		uint32_t zoneId{};    // world servers: from the command line
		uint32_t instanceId{};
	};

	// Main thread: every server process of this build (Linux: /proc; elsewhere none)
	std::vector<Process> Processes();
}
