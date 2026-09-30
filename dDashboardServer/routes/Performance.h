#pragma once

#include <string>

#include "Profiler.h"

struct ProfileResult;

/**
 * The Performance page (docs/Dashboard.md, "Performance"): every server's main loop frame times and phases, its worst
 * and slow frames, and profiling sessions with a flame graph. Frame timing comes with the traffic reports; sessions go
 * out as PROFILE_REQUEST through master and come back as PROFILE_RESULT.
 */
namespace Performance {
	void RegisterRoutes();

	// A traffic report's frames section (Traffic::Ingest passes it on)
	void Ingest(const std::string& serverKey, const Profiler::Report& frames);

	// PROFILE_RESULT from a server (via master), or the dashboard's own
	void IngestProfile(const ProfileResult& result);

	// Main loop: forget old seconds, give up on sessions that never answered
	void Update();
}
