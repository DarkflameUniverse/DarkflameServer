#pragma once

/**
 * Instance load: players in each world instance over time (sampled once a minute alongside the server health, kept
 * for health_days), and per-zone player caps and spare instances that the master server applies.
 */
namespace InstanceLoad {
	void RegisterRoutes();

	// Main loop: take a sample every minute
	void Update();
}
