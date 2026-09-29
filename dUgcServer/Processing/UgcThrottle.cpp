#include "UgcThrottle.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>

namespace {
	// Up to this much CPU time may be used ahead of the budget (so short jobs aren't slowed at all)
	constexpr double BURST_SECONDS = 0.25;
	// Checkpoints closer together than this only read the clock
	constexpr double MIN_ACCOUNT_SECONDS = 0.005;

	std::atomic<double> g_Budget{ 0.0 };
	std::atomic<bool> g_Cancel{ false };
	std::mutex g_Mutex;
	double g_Balance = BURST_SECONDS; // CPU seconds that may still be used
	std::chrono::steady_clock::time_point g_Refilled = std::chrono::steady_clock::now();
	std::atomic<uint64_t> g_SleptMs{ 0 };
	std::atomic<int64_t> g_LastSleep{ 0 };

	thread_local double t_LastCpu = -1.0;
	thread_local double t_Charged = 0.0; // CPU seconds libraries used for this thread on threads of their own

	int64_t UnixMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void Refill(double budget) {
		const auto now = std::chrono::steady_clock::now();
		const double elapsed = std::chrono::duration<double>(now - g_Refilled).count();
		g_Refilled = now;
		g_Balance = std::min(g_Balance + elapsed * budget, BURST_SECONDS * std::max(budget, 1.0));
	}
}

namespace UgcThrottle {
	void SetBudget(double cpus) {
		std::lock_guard lock(g_Mutex);
		g_Budget = std::max(cpus, 0.0);
		g_Balance = std::min(g_Balance, BURST_SECONDS);
		g_Refilled = std::chrono::steady_clock::now();
	}

	double GetBudget() {
		return g_Budget;
	}

	double ThreadCpuSeconds() {
#if defined(CLOCK_THREAD_CPUTIME_ID)
		timespec ts{};
		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;
#endif
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void Charge(double seconds) {
		if (!(seconds > 0.0)) return;
		t_Charged += seconds;
		// The next Checkpoint sees it as used since the last
		if (t_LastCpu >= 0.0) t_LastCpu -= seconds;
	}

	double JobCpuSeconds() {
		return ThreadCpuSeconds() + t_Charged;
	}

	void Begin() {
		t_LastCpu = ThreadCpuSeconds();
	}

	void Cancel(const bool cancel) { g_Cancel = cancel; }
	bool IsCancelled() { return g_Cancel; }

	void Checkpoint() {
		Checkpoint({});
	}

	void Checkpoint(const std::function<void(bool)>& pausing) {
		if (g_Cancel) throw Cancelled{};
		const double budget = g_Budget;
		if (budget <= 0.0) return;
		const double cpu = ThreadCpuSeconds();
		if (t_LastCpu < 0.0) t_LastCpu = cpu;
		const double used = cpu - t_LastCpu;
		if (used < MIN_ACCOUNT_SECONDS) return;
		t_LastCpu = cpu;

		double wait = 0.0;
		{
			std::lock_guard lock(g_Mutex);
			Refill(budget);
			g_Balance -= used;
			// Overdrawn: wait until the budget has paid it back. Other threads waiting meanwhile each owe their own
			// share, so the waits add up to what the budget allows.
			if (g_Balance < 0.0) wait = -g_Balance / budget;
		}
		if (wait <= 0.0) return;
		wait = std::min(wait, 5.0);
		g_SleptMs += static_cast<uint64_t>(wait * 1000.0);
		g_LastSleep = UnixMs();
		// In short sleeps, so a cancel isn't held up by a long wait
		const auto until = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(wait));
		if (pausing) pausing(true);
		while (std::chrono::steady_clock::now() < until) {
			if (g_Cancel) {
				if (pausing) pausing(false);
				throw Cancelled{};
			}
			std::this_thread::sleep_for(std::min<std::chrono::steady_clock::duration>(until - std::chrono::steady_clock::now(), std::chrono::milliseconds(100)));
		}
		if (pausing) pausing(false);
		// Time asleep costs no CPU; don't count this call's own bookkeeping twice
		t_LastCpu = ThreadCpuSeconds();
	}

	Stats GetStats() {
		return { g_SleptMs.load(), g_LastSleep.load() };
	}

	bool ParseHours(const std::string& text, int& from, int& to) {
		const auto dash = text.find('-');
		if (dash == std::string::npos) return false;
		try {
			const int a = std::stoi(text.substr(0, dash)), b = std::stoi(text.substr(dash + 1));
			if (a < 0 || a > 23 || b < 0 || b > 23) return false;
			from = a;
			to = b;
			return true;
		} catch (...) {
			return false;
		}
	}

	bool InHours(int hour, int from, int to) {
		if (from < 0 || to < 0) return false;
		return from <= to ? hour >= from && hour <= to : hour >= from || hour <= to;
	}
}
