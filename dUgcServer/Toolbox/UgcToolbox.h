#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "json.hpp"
#include "UgcToolboxProtocol.h"

/**
 * The processor=toolbox-blender option (docs/UgcServer.md, "LU Toolbox in Blender"): models made by LU Toolbox itself
 * in a headless Blender, an external program the UGC server starts and talks to (never linked in). One Blender stays up
 * and makes one model after another (dlu_toolbox_worker.py, which runs LU-Toolbox-Standalone's steps); it is started
 * when first needed, again after it crashes or hangs, and after `restartAfterJobs` models. The UGC workers take turns
 * on it. Linux and other POSIX systems only for now.
 */
namespace UgcToolbox {
	struct Config {
		std::filesystem::path blender;        // toolbox_blender: the Blender executable
		std::filesystem::path standalone;     // toolbox_standalone_dir: LU-Toolbox-Standalone (lu_batch_driver.py)
		std::filesystem::path scripts;        // toolbox_scripts_dir: a Blender scripts folder whose addons/ has lu_toolbox and
		                                      // io_scene_niftools (empty: the add-ons installed in Blender's own user folder)
		std::filesystem::path worker;         // dlu_toolbox_worker.py (next to the servers, ugc-toolbox/)
		std::filesystem::path brickdb;        // toolbox_brickdb_dir: LU Toolbox's brick folder, made from `res` by the worker
		std::filesystem::path res;            // the client's res folder
		std::filesystem::path work;           // toolbox_work_dir: the LXFML and .nif of the model being made, Blender's log
		std::string device{ "cpu" };          // toolbox_device: cpu, cuda, optix, hip or auto (Cycles, for the bakes)
		uint32_t threads{ 4 };                // toolbox_threads: Blender's threads (-t)
		uint32_t timeoutSeconds{ 1800 };      // toolbox_timeout_seconds: a model taking longer fails and Blender is restarted
		uint32_t restartAfterJobs{ 100 };     // Blender is started again after this many models (0: never)
		int nice{};                           // worker_nice

		bool operator==(const Config&) const = default;
	};

	inline constexpr std::string_view DEVICES[] = { "cpu", "cuda", "optix", "hip", "auto" };

	/**
	 * Why toolbox-blender can't be used with `config` (empty: it can, as far as can be told without starting Blender): a
	 * missing setting, program, folder or add-on. Reads the filesystem only.
	 */
	std::string Problem(const Config& config);

	/**
	 * The processor a make uses: `wanted` (UgcProcessOptions::PROCESSOR) when it can be, else native; `why` gets the
	 * reason when it falls back. Unknown names are native too.
	 */
	std::string_view Resolve(std::string_view wanted, const std::string& problem, std::string& why);

	struct Result {
		bool ok{};
		std::string error;
		std::string nif;          // the .nif made
		nlohmann::json ms;        // the worker's steps' times: reset, import, process, bake, export
		double blenderCpuSeconds{}; // Blender's CPU time for it (all its threads)
		double waitedMs{};        // waiting for Blender (another model, or it starting)
		bool started{};           // Blender was started for it
		std::string blender;      // "Blender 3.1.2, LU Toolbox 2.4.0, niftools 0.1.1, cpu"
	};

	/**
	 * The Blender worker. Make is called by the UGC workers (any thread; they take turns); Configure and Stop by the main
	 * thread.
	 */
	class Worker {
	public:
		Worker() = default;
		~Worker();
		Worker(const Worker&) = delete;
		Worker& operator=(const Worker&) = delete;

		// New settings: used from the next model on (Blender is started again when they changed)
		void Configure(const Config& config);
		Config GetConfig() const;

		/**
		 * Makes a model's .nif from its LXFML with LU Toolbox, importing `lods`. `pace` is called about every 100 ms while
		 * Blender works, with the CPU seconds it used since the last call and a function that pauses (true) or resumes
		 * (false) it: the CPU budget (UgcThrottle) is kept through it, and it may throw to give up (the server stopping),
		 * which stops Blender. `id` names the files in the work folder.
		 */
		using Pause = std::function<void(bool)>;
		Result Make(uint64_t id, const std::string& lxfml, const std::vector<uint32_t>& lods,
			const std::function<void(double cpuSeconds, const Pause& pause)>& pace = {});

		// Stops Blender (asks it to quit, then kills it); the next Make starts it again
		void Stop();

		// For /status: running, pid, models made by this Blender, starts, the versions it reported, the last error
		nlohmann::json Status() const;

	private:
		struct Process;
		bool StartLocked(std::string& error);
		void StopLocked(bool kill);
		std::string LogTail() const;

		mutable std::mutex m_ConfigMutex;
		Config m_Config;
		std::mutex m_JobMutex; // one model at a time; guards everything below
		Process* m_Process{};
		Config m_Running;      // the settings the running Blender was started with
		uint64_t m_Jobs{};     // made by the running Blender
		uint64_t m_Starts{};
		uint64_t m_NextId{ 1 };
		std::string m_Versions;
		std::string m_LastError;
		uint32_t m_FailedStarts{};
		std::chrono::steady_clock::time_point m_RetryAfter{};
		mutable std::mutex m_StatusMutex;
		nlohmann::json m_Status = nlohmann::json::object();
		void PublishStatus();
	};
}
