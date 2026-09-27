#pragma once

#include <cstdint>
#include <string>

/**
 * Keeps the UGC workers' CPU use under a budget. The long loops (renders, ambient occlusion, icons) call Checkpoint
 * every few milliseconds of work; it adds the calling thread's CPU time since its last call to a shared account that
 * the budget fills at `budget` CPUs per second, and sleeps while the account is overdrawn. So the workers together
 * average at most the budget however many there are, long jobs included, and without a budget it costs nothing.
 */
namespace UgcThrottle {
	// CPUs the workers may use together on average (1.5: one and a half cores); 0 or less: no limit
	void SetBudget(double cpus);
	double GetBudget();

	// Account the calling thread's CPU time and sleep when over the budget
	void Checkpoint();

	// Start accounting on this thread from now (a worker starting a job), so time spent idle isn't counted
	void Begin();

	struct Stats {
		uint64_t sleptMs{};          // since start
		int64_t lastSleepUnixMs{};   // when it last slept, 0 never
	};
	Stats GetStats();

	// This thread's CPU time in seconds
	double ThreadCpuSeconds();

	// Parses "18-23" (from 18:00 to 23:59; may wrap past midnight, "22-6") into from and to; false when it isn't that
	bool ParseHours(const std::string& text, int& from, int& to);
	// Whether `hour` (0-23) is inside the range; never when from or to is negative
	bool InHours(int hour, int from, int to);
}
