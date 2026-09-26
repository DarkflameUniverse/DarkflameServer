#pragma once

#include <cstdint>
#include <functional>
#include <string>

/**
 * Daily economy jobs, registered as scheduled tasks (see Scheduler.h): anomaly checks that raise flags (unusual coin
 * income, item spikes, duplicated items) for the day that just ended, keeping the ledger's size in check (merging old
 * daily rows into months, dropping old transfers), and pruning old log rows.
 */
void RegisterEconomyJobRoutes();

// Before Scheduler::Initialize
void RegisterEconomyTasks();

struct EconomyCheckResult {
	uint32_t income{};
	uint32_t items{};
	uint32_t duplicates{};
	bool duplicateScan{}; // whether the duplicate scan ran
	uint32_t collisionsDismissed{}; // open duplicate flags dismissed because the id is shared by different items
	uint32_t collisions{}; // new flags for id collisions the login migration will not fix
	std::string error;    // empty on success

	uint32_t Total() const { return income + items + duplicates + collisions; }
};

// Run the anomaly checks for one day in the background; done runs on the main thread
void RunEconomyChecks(uint32_t day, bool duplicateScan, std::function<void(const EconomyCheckResult&)> done);
