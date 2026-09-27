#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "dCommonVars.h"
#include "json_fwd.hpp"
#include "UgcBricks.h"
#include "UgcJobs.h"
#include "UgcStorage.h"

/**
 * The UGC server's queue. The main thread (Update) takes pending rows from the database, gathers each one's input
 * (the stored LXFML, or a modular build's modules from the CDClient) and hands it to the worker threads; they make
 * and write the files; the main thread records the outcome in the database. Workers never touch the database, the
 * CDClient or the network.
 */
class UgcProcessor {
public:
	using Kind = UgcStorage::Kind;

	struct Config {
		uint32_t pollIntervalMs{ 2000 };
		uint32_t pollBatch{ 32 };
		uint32_t maxAttempts{ 3 };
		uint64_t maxStorageBytes{}; // 0: no limit
		size_t threads{ 2 };
	};

	UgcProcessor(Config config, UgcStorage& storage, UgcBricks::BrickLibrary& library, UgcJobs::Settings settings);
	~UgcProcessor();

	void Start();
	// Waits for the jobs that are running; queued ones are dropped (they stay pending in the database)
	void Stop();

	// Main thread: poll, dispatch, record results, keep the storage under its cap
	void Update();

	enum class Availability {
		READY,   // the files are there
		QUEUED,  // it exists and is being made (again)
		UNKNOWN, // no such item, or it failed
	};

	// Main thread: whether an item's files can be served, queuing it to be made when it exists but they're missing
	Availability Request(Kind kind, LWOOBJID id);

	// Main thread: what the server is doing, for /status
	nlohmann::json Status() const;

	// Any thread: jobs waiting for a worker, and workers busy (traffic diagnostics)
	size_t Queued() const { std::lock_guard lock(m_Mutex); return m_Jobs.size(); }
	size_t Busy() const { std::lock_guard lock(m_Mutex); return m_Active; }
	size_t Threads() const { return m_Config.threads; }

private:
	struct Job {
		Kind kind{};
		LWOOBJID id{};
		uint32_t attempts{};
		std::string blob;               // models: the stored LXFML
		UgcJobs::ModularInput modular;  // modular builds
	};

	struct Done {
		Kind kind{};
		LWOOBJID id{};
		uint32_t attempts{};
		UgcJobs::Outcome outcome;
		uint64_t bytes{};
		double milliseconds{};
	};

	void Poll();
	void Collect();
	void Record(const Done& done);
	void Worker();

	Config m_Config;
	UgcStorage& m_Storage;
	UgcBricks::BrickLibrary& m_Library;
	UgcJobs::Settings m_Settings;

	mutable std::mutex m_Mutex;
	std::condition_variable m_Wake;
	std::deque<Job> m_Jobs;
	std::deque<Done> m_Done;
	size_t m_Active{};
	bool m_Stopping{};
	std::vector<std::thread> m_Threads;

	// Main thread only
	std::set<std::pair<Kind, LWOOBJID>> m_InFlight;
	std::chrono::steady_clock::time_point m_NextPoll{};
	std::chrono::steady_clock::time_point m_NextEviction{};
	std::map<std::pair<Kind, LWOOBJID>, std::pair<std::chrono::steady_clock::time_point, Availability>> m_Recent; // answers for missing files
	uint64_t m_StoredBytes{};
	uint64_t m_Made{};
	uint64_t m_Failed{};
	uint64_t m_Evicted{};
	struct LogEntry {
		Kind kind{};
		LWOOBJID id{};
		bool ok{};
		double milliseconds{};
		std::string message;
		int64_t time{};
	};
	std::deque<LogEntry> m_Log; // the last results
};
