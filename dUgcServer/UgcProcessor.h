#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "DeferredReply.h"
#include "dCommonVars.h"
#include "json_fwd.hpp"
#include "UgcBricks.h"
#include "UgcIconParams.h"
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

	/**
	 * What the workers may use; changed while running (Configure) when the settings are reloaded.
	 */
	struct Limits {
		double maxCpus{};            // CPUs the workers may use together on average (UgcThrottle); 0: no limit
		uint64_t maxMemoryBytes{};   // estimated memory of the jobs running at once; 0: no limit
		int nice{};                  // the workers' scheduling priority (Linux nice, 0 to 19)
		int pauseFromHour{ -1 };     // local hours in which no new jobs start (from, to; -1: never)
		int pauseToHour{ -1 };
	};

	UgcProcessor(Config config, UgcStorage& storage, UgcBricks::BrickLibrary& library, UgcJobs::Settings settings);
	~UgcProcessor();

	void Start();

	// Main thread: new settings and limits (config reload)
	void Configure(UgcJobs::Settings settings, Limits limits);
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

	// Main thread: the id an item's files are stored under. A car or rocket's are its combination's (every build of
	// the same modules shares one icon); 0 when a build's modules can't be told.
	LWOOBJID StorageId(Kind kind, LWOOBJID id);

	/**
	 * Main thread: renders an icon with `values` (UgcIconParams, over the settings) without storing it, on a worker
	 * (within the budgets, ahead of the queue); `reply` gets the PNG. A player model's is drawn from its stored .nif, a
	 * car or rocket's from `modules`. False (and `error`) when there's nothing to draw from.
	 */
	bool QueuePreview(Kind kind, LWOOBJID id, const std::string& modules, const UgcIconParams::Values& values, DeferredReply reply, std::string& error);

	/**
	 * Main thread: the assembled mesh of a module combination as a .nif (UgcJobs::AssemblyNif), for the dashboard's
	 * pose editor: from a small cache of the last ones asked for, else made on a worker (within the budgets, ahead of
	 * the queue) and cached. `reply` gets the .nif. False (and `error`) when the modules can't be told.
	 */
	bool QueueAssembly(const std::string& modules, DeferredReply reply, std::string& error);

	// Main thread: queues every stored icon of a kind ("model", or "build<type>" for cars and rockets) to be drawn again
	// with the current settings, presets and overrides; only icons (models' from their stored .nif). How many.
	size_t RegenerateIcons(const std::string& kind);

	enum class eAfterDelete {
		ON_DEMAND, // rows stay made; the files are made again when something asks for them
		NOW,       // rows go back to waiting, so they're made again soon
		GONE,      // rows are marked failed ("deleted"), so they aren't made again until someone asks for that
	};

	struct DeleteRequest {
		Kind kind{};
		std::vector<LWOOBJID> ids;   // models, or builds (their combinations are deleted); empty with `all`
		bool all{};
		int64_t olderThanDays{};     // only files made longer ago than this (0: any)
		int64_t unusedDays{};        // only files not asked for in this long (0: any)
		eAfterDelete after{ eAfterDelete::ON_DEMAND };
	};

	struct DeleteResult {
		size_t deleted{};
		uint64_t bytes{};
		size_t busy{}; // being made right now, left alone
		std::vector<std::string> notes;
	};

	// Main thread: deletes stored files. Items being made are skipped, so a worker never writes what is being deleted.
	DeleteResult Delete(const DeleteRequest& request);

	// Main thread: what the server is doing, for /status
	nlohmann::json Status() const;

	// Any thread: jobs waiting for a worker, and workers busy (traffic diagnostics)
	size_t Queued() const { std::lock_guard lock(m_Mutex); return m_Jobs.size(); }
	size_t Busy() const { std::lock_guard lock(m_Mutex); return m_Active; }
	size_t Threads() const { return m_Config.threads; }
	// Main thread: the process's CPU use (percent of one core) and resident memory, and the running jobs' estimate
	double CpuPercent() const { return m_CpuPercent; }
	static uint64_t ResidentBytes();
	uint64_t JobMemory() const { std::lock_guard lock(m_Mutex); return m_MemoryInUse; }
	bool Throttled() const;

	// Main thread: totals since the start and the files' size (traffic reports, then the dashboard and /metrics)
	uint64_t Made() const { return m_Made; }
	uint64_t Failed() const { return m_Failed; }
	uint64_t Evicted() const { return m_Evicted; }
	uint64_t StoredBytes() const { return m_StoredBytes; }
	uint64_t MaxStorageBytes() const { return m_Config.maxStorageBytes; }

	// Main thread: the models whose mesh (model.nif) was written with a different checksum than before since the last
	// call, for the worlds (UGC_MODELS_MADE); a model made again unchanged (e.g. after eviction) isn't listed
	std::vector<LWOOBJID> TakeChangedMeshes() { std::vector<LWOOBJID> ids; ids.swap(m_ChangedMeshes); return ids; }

private:
	struct Job {
		Kind kind{};
		LWOOBJID id{};
		uint32_t attempts{};
		std::string blob;               // models: the LXFML
		UgcJobs::ModularInput modular;  // modular builds
		uint64_t memory{};              // estimated bytes it needs
		size_t parts{};
		DeferredReply preview;          // an icon preview: answered with the PNG, nothing stored
		bool iconOnly{};                // a model's icon drawn again from its stored .nif
		bool assembly{};                // with `preview`: answered with the assembled .nif instead of an icon
		UgcIconParams::Values iconValues; // models: the preset and override (UgcIconParams)
	};

	// A written file's MD5 and size as the client has it after inflating it (from its .checksum)
	struct Checksum {
		std::string file; // icon.dds, model.nif
		std::string md5;
		uint32_t size{};
	};

	struct Done {
		Kind kind{};
		LWOOBJID id{};
		uint32_t attempts{};
		UgcJobs::Outcome outcome;
		uint64_t bytes{};
		double milliseconds{};
		double cpuMilliseconds{};  // the worker thread's CPU time for it
		uint64_t memoryEstimate{}; // the bytes it was estimated to need (the memory budget's figure)
		bool iconOnly{};
		std::vector<Checksum> checksums; // of the files written that the client downloads as sd0
	};

	// Main thread: records the checksums of an item's files (ugc_file_checksums)
	void StoreChecksums(Kind kind, LWOOBJID storageId, const std::vector<Checksum>& checksums);

	// Main thread: fills in what was made before the worlds answered manifest requests, a little per Update: the
	// icons' sd0 files and checksums of the items stored, and the builds' combination ids
	void Backfill();
	std::deque<UgcStorage::Entry> m_BackfillItems;
	bool m_BackfillItemsDone{ true };
	std::deque<LWOOBJID> m_StatsBackfill; // models whose triangle counts are read from their stats.json once
	bool m_StatsBackfillDone{ true };
	bool m_BackfillBuildsDone{ false };

	void Poll();
	// Main thread: the icon values for a kind and an item (the kind's preset, then the item's override)
	UgcIconParams::Values IconValues(const std::string& kind, const std::string& itemTarget);
	void Collect();
	void Record(const Done& done);
	void Worker();
	void SampleUsage();

	Config m_Config;
	UgcStorage& m_Storage;
	UgcBricks::BrickLibrary& m_Library;
	UgcJobs::Settings m_Settings; // guarded by m_Mutex (workers copy it per job)
	Limits m_Limits;              // guarded by m_Mutex

	mutable std::mutex m_Mutex;
	std::condition_variable m_Wake;
	std::deque<Job> m_Jobs;
	std::deque<Done> m_Done;
	size_t m_Active{};
	uint64_t m_MemoryInUse{};  // estimates of the running jobs
	uint64_t m_Waiting{};      // times a job waited for memory
	bool m_Paused{};           // main thread: in the pause hours
	// Main thread: process CPU use, measured between status reads
	std::chrono::steady_clock::time_point m_CpuSampled{};
	double m_CpuSeconds{};
	double m_CpuPercent{};
	bool m_Stopping{};
	std::vector<std::thread> m_Threads;

	// Main thread only
	std::set<std::pair<Kind, LWOOBJID>> m_InFlight;        // models, and builds waiting for their combination
	std::set<LWOOBJID> m_ComboJobs;                         // combinations being made
	std::map<LWOOBJID, std::vector<std::pair<LWOOBJID, uint32_t>>> m_ComboRows; // combination -> builds (id, attempts) waiting for it
	std::map<LWOOBJID, LWOOBJID> m_ComboOf;                 // build -> combination (StorageId)
	uint64_t m_Reused{};
	uint64_t m_Empty{};  // models with no bricks (nothing to make)                                    // builds whose combination was made already
	std::chrono::steady_clock::time_point m_NextPoll{};
	std::chrono::steady_clock::time_point m_NextEviction{};
	std::map<std::pair<Kind, LWOOBJID>, std::pair<std::chrono::steady_clock::time_point, Availability>> m_Recent; // answers for missing files
	uint64_t m_StoredBytes{};
	std::vector<LWOOBJID> m_ChangedMeshes; // TakeChangedMeshes
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

	// Assembled meshes (QueueAssembly) by combination, newest first; workers add to it
	std::mutex m_AssemblyMutex;
	std::list<std::pair<std::string, std::shared_ptr<const std::string>>> m_Assemblies;
	std::shared_ptr<const std::string> CachedAssembly(const std::string& key);
	void CacheAssembly(const std::string& key, std::shared_ptr<const std::string> nif);
};
