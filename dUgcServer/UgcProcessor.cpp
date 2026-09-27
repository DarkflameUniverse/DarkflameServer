#include "UgcProcessor.h"

#include "CDClientDatabase.h"
#include "Database.h"
#include "Logger.h"
#include "UgcCdClient.h"
#include "UgcThrottle.h"
#include "json.hpp"

#include <ctime>
#include <fstream>
#include <thread>
#if defined(__linux__)
#include <malloc.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
	constexpr size_t MAX_ERROR_LENGTH = 1000; // process_error holds 1024
	constexpr size_t LOG_LENGTH = 50;
	constexpr auto EVICTION_INTERVAL = std::chrono::minutes(5);
	constexpr auto RECENT_ANSWER_TIME = std::chrono::seconds(10);

	const char* KindName(UgcStorage::Kind kind) {
		return kind == UgcStorage::Kind::MODEL ? "model" : "modular";
	}

	// The process's CPU time (user and system, every thread), seconds
	double ProcessCpuSeconds() {
		timespec ts{};
		if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0) return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;
		return 0.0;
	}

	void ApplyNice(int nice) {
#if defined(__linux__)
		// Per thread on Linux: only the calling worker's priority changes
		setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), std::clamp(nice, 0, 19));
#else
		(void)nice;
#endif
	}

	int64_t UnixNow() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

UgcProcessor::UgcProcessor(Config config, UgcStorage& storage, UgcBricks::BrickLibrary& library, UgcJobs::Settings settings)
	: m_Config(config), m_Storage(storage), m_Library(library), m_Settings(std::move(settings)) {}

uint64_t UgcProcessor::ResidentBytes() {
#if defined(__linux__)
	std::ifstream statm("/proc/self/statm");
	uint64_t size = 0, resident = 0;
	if (statm >> size >> resident) return resident * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
#endif
	return 0;
}

bool UgcProcessor::Throttled() const {
	const auto last = UgcThrottle::GetStats().lastSleepUnixMs;
	return last > 0 && UnixNow() * 1000 - last < 5000;
}

void UgcProcessor::Configure(UgcJobs::Settings settings, Limits limits) {
	{
		std::lock_guard lock(m_Mutex);
		m_Settings = std::move(settings);
		m_Limits = limits;
	}
	UgcThrottle::SetBudget(limits.maxCpus);
	m_Wake.notify_all();
}

void UgcProcessor::SampleUsage() {
	const auto now = std::chrono::steady_clock::now();
	const double cpu = ProcessCpuSeconds();
	if (m_CpuSampled.time_since_epoch().count() != 0) {
		const double elapsed = std::chrono::duration<double>(now - m_CpuSampled).count();
		if (elapsed > 0.0) m_CpuPercent = (cpu - m_CpuSeconds) / elapsed * 100.0;
	}
	m_CpuSampled = now;
	m_CpuSeconds = cpu;
}

UgcProcessor::~UgcProcessor() {
	Stop();
}

void UgcProcessor::Start() {
	for (const auto& entry : m_Storage.List()) m_StoredBytes += entry.bytes;
	m_NextEviction = std::chrono::steady_clock::now();
	m_Stopping = false;
	for (size_t i = 0; i < std::max<size_t>(m_Config.threads, 1); i++) m_Threads.emplace_back(&UgcProcessor::Worker, this);
	LOG("UGC processing started with %zu worker(s), %llu MB stored", m_Threads.size(), static_cast<unsigned long long>(m_StoredBytes / (1024 * 1024)));
}

void UgcProcessor::Stop() {
	{
		std::lock_guard lock(m_Mutex);
		m_Stopping = true;
		m_Jobs.clear();
	}
	m_Wake.notify_all();
	for (auto& thread : m_Threads) {
		if (thread.joinable()) thread.join();
	}
	m_Threads.clear();
}

void UgcProcessor::Worker() {
	int appliedNice = 0;
	while (true) {
		Job job;
		UgcJobs::Settings settings;
		int nice = 0;
		{
			std::unique_lock lock(m_Mutex);
			// The first job that fits the memory budget beside the ones running; one that is too big alone runs
			// when nothing else does
			auto pick = m_Jobs.end();
			m_Wake.wait(lock, [this, &pick] {
				if (m_Stopping) return true;
				pick = m_Jobs.end();
				for (auto it = m_Jobs.begin(); it != m_Jobs.end(); ++it) {
					if (m_Limits.maxMemoryBytes == 0 || m_Active == 0 || m_MemoryInUse + it->memory <= m_Limits.maxMemoryBytes) {
						pick = it;
						break;
					}
				}
				if (pick == m_Jobs.end() && !m_Jobs.empty()) m_Waiting++;
				return pick != m_Jobs.end();
			});
			if (m_Stopping) return;
			job = std::move(*pick);
			m_Jobs.erase(pick);
			m_Active++;
			m_MemoryInUse += job.memory;
			settings = m_Settings;
			nice = m_Limits.nice;
		}
		if (nice != appliedNice) {
			ApplyNice(nice);
			appliedNice = nice;
		}

		UgcThrottle::Begin();
		const auto start = std::chrono::steady_clock::now();
		Done done{ job.kind, job.id, job.attempts };
		try {
			done.outcome = job.kind == Kind::MODEL
				? UgcJobs::ProcessModel(job.blob, m_Library, settings, static_cast<uint64_t>(job.id))
				: UgcJobs::ProcessModular(job.modular, m_Library.GetResPath(), settings);
		} catch (const std::exception& ex) {
			done.outcome.ok = false;
			done.outcome.error = std::string("crashed: ") + ex.what();
		}
		job.blob.clear();
		job.blob.shrink_to_fit();
		if (done.outcome.ok) {
			std::string error;
			const auto bytes = m_Storage.Write(job.kind, job.id, done.outcome.files, error);
			if (bytes) {
				done.bytes = *bytes;
			} else {
				done.outcome.ok = false;
				done.outcome.error = error;
			}
		}
		done.outcome.files.clear();
		done.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		UgcThrottle::Checkpoint();

		{
			std::lock_guard lock(m_Mutex);
			m_Active--;
			m_MemoryInUse -= std::min(m_MemoryInUse, job.memory);
			m_Done.push_back(std::move(done));
		}
#if defined(__linux__) && defined(__GLIBC__)
		// Give the big buffers of the job back to the system
		malloc_trim(0);
#endif
		m_Wake.notify_all();
	}
}

void UgcProcessor::Poll() {
	size_t queued = 0, active = 0;
	Limits limits;
	UgcJobs::Settings settings;
	{
		std::lock_guard lock(m_Mutex);
		queued = m_Jobs.size();
		active = m_Active;
		limits = m_Limits;
		settings = m_Settings;
	}
	const auto now = std::time(nullptr);
	std::tm local{};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	m_Paused = UgcThrottle::InHours(local.tm_hour, limits.pauseFromHour, limits.pauseToHour);
	if (m_Paused) return;
	// Enough to keep every worker busy until the next poll
	const size_t wanted = std::max<size_t>(m_Threads.size() * 2, m_Config.pollBatch);
	if (queued + active >= wanted) return;
	const auto limit = static_cast<uint32_t>(wanted - queued - active + m_InFlight.size());

	std::vector<Job> jobs;
	for (auto& model : Database::Get()->GetUgcModelsToProcess(limit)) {
		if (m_InFlight.contains({ Kind::MODEL, model.id })) continue;
		Job job{ Kind::MODEL, model.id, model.attempts, UgcJobs::LxfmlFromBlob(model.lxfml) };
		if (job.blob.empty()) job.blob = std::move(model.lxfml); // the worker reports it can't be read
		job.parts = UgcJobs::CountParts(job.blob);
		job.memory = UgcJobs::EstimateMemory(job.parts, settings);
		jobs.push_back(std::move(job));
	}
	for (auto& build : Database::Get()->GetModularBuildsToProcess(limit)) {
		if (m_InFlight.contains({ Kind::MODULAR, build.id })) continue;
		Job job{ Kind::MODULAR, build.id, build.attempts };
		job.memory = UgcJobs::EstimateMemory(64, settings);
		std::string error;
		if (!UgcCdClient::GatherModular(build.modules, job.modular, error)) {
			// Nothing a worker could do: record it right away
			Record(Done{ Kind::MODULAR, build.id, build.attempts, UgcJobs::Outcome{ false, error } });
			continue;
		}
		jobs.push_back(std::move(job));
	}
	if (jobs.empty()) return;
	{
		std::lock_guard lock(m_Mutex);
		for (auto& job : jobs) {
			m_InFlight.insert({ job.kind, job.id });
			m_Jobs.push_back(std::move(job));
		}
	}
	m_Wake.notify_all();
}

void UgcProcessor::Record(const Done& done) {
	auto error = done.outcome.error.substr(0, MAX_ERROR_LENGTH);
	const auto attempts = done.attempts + 1;
	const auto state = done.outcome.ok ? IUgc::eProcessState::DONE
		: attempts >= m_Config.maxAttempts ? IUgc::eProcessState::FAILED : IUgc::eProcessState::PENDING;
	if (done.kind == Kind::MODEL) {
		Database::Get()->SetUgcModelProcessed(done.id, state, attempts, error, done.outcome.ok && done.outcome.aoBaked);
	} else {
		Database::Get()->SetModularBuildProcessed(done.id, state, attempts, error);
	}
	m_Recent.erase({ done.kind, done.id });

	if (done.outcome.ok) {
		m_Made++;
		m_StoredBytes += done.bytes;
		LOG_DEBUG("Made %s %llu in %.0f ms%s%s", KindName(done.kind), static_cast<unsigned long long>(done.id), done.milliseconds,
			done.outcome.note.empty() ? "" : ": ", done.outcome.note.c_str());
	} else {
		m_Failed++;
		LOG("Couldn't make %s %llu (attempt %u): %s", KindName(done.kind), static_cast<unsigned long long>(done.id), attempts, error.c_str());
	}
	m_Log.push_back({ done.kind, done.id, done.outcome.ok, done.milliseconds, done.outcome.ok ? done.outcome.note : error, UnixNow() });
	while (m_Log.size() > LOG_LENGTH) m_Log.pop_front();
}

void UgcProcessor::Collect() {
	std::deque<Done> finished;
	{
		std::lock_guard lock(m_Mutex);
		finished.swap(m_Done);
	}
	for (const auto& done : finished) {
		m_InFlight.erase({ done.kind, done.id });
		Record(done);
	}
	// Something finished: look for more now rather than at the next interval
	if (!finished.empty()) m_NextPoll = std::min(m_NextPoll, std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
}

void UgcProcessor::Update() {
	Collect();
	const auto now = std::chrono::steady_clock::now();
	if (now - m_CpuSampled >= std::chrono::seconds(2)) SampleUsage();
	if (now >= m_NextPoll) {
		m_NextPoll = now + std::chrono::milliseconds(m_Config.pollIntervalMs);
		Poll();
	}
	if (m_Config.maxStorageBytes > 0 && (m_StoredBytes > m_Config.maxStorageBytes || now >= m_NextEviction)) {
		m_NextEviction = now + EVICTION_INTERVAL;
		// Deleted files stay marked made: they're made again when someone asks for them (Request)
		const auto removed = m_Storage.Evict(m_Config.maxStorageBytes);
		m_StoredBytes = 0;
		for (const auto& entry : m_Storage.List()) m_StoredBytes += entry.bytes;
		if (!removed.empty()) {
			m_Evicted += removed.size();
			LOG("Deleted the files of %zu item(s) used longest ago to stay under %llu MB", removed.size(), static_cast<unsigned long long>(m_Config.maxStorageBytes / (1024 * 1024)));
		}
	}
	// Forget old answers
	std::erase_if(m_Recent, [now](const auto& item) { return now - item.second.first > RECENT_ANSWER_TIME; });
}

UgcProcessor::Availability UgcProcessor::Request(Kind kind, LWOOBJID id) {
	if (m_InFlight.contains({ kind, id })) return Availability::QUEUED;
	if (m_Storage.File(kind, id, "icon.png")) {
		m_Storage.Touch(kind, id);
		return Availability::READY;
	}
	if (const auto it = m_Recent.find({ kind, id }); it != m_Recent.end()) return it->second.second;

	const auto info = kind == Kind::MODEL ? Database::Get()->GetUgcProcessInfo(id) : Database::Get()->GetModularBuildProcessInfo(id);
	Availability answer = Availability::UNKNOWN;
	if (info && info->state != IUgc::eProcessState::FAILED) {
		// Made before but the files are gone (deleted to save space): make them again, first
		if (info->state == IUgc::eProcessState::DONE) {
			if (kind == Kind::MODEL) Database::Get()->ResetUgcModelProcessing(id, false);
			else Database::Get()->ResetModularBuildProcessing(id, false);
		}
		m_NextPoll = std::chrono::steady_clock::now();
		answer = Availability::QUEUED;
	}
	m_Recent[{ kind, id }] = { std::chrono::steady_clock::now(), answer };
	return answer;
}

nlohmann::json UgcProcessor::Status() const {
	nlohmann::json status;
	{
		std::lock_guard lock(m_Mutex);
		status["queued"] = m_Jobs.size();
		status["active"] = m_Active;
	}
	Limits limits;
	{
		std::lock_guard lock(m_Mutex);
		limits = m_Limits;
		status["jobMemoryBytes"] = m_MemoryInUse;
		status["memoryWaits"] = m_Waiting;
	}
	status["workers"] = m_Threads.size();
	const auto throttle = UgcThrottle::GetStats();
	status["usage"] = {
		{ "cpuPercent", std::lround(m_CpuPercent) }, // of one core
		{ "cores", std::thread::hardware_concurrency() },
		{ "residentBytes", ResidentBytes() },
		{ "throttled", Throttled() },
		{ "throttledMs", throttle.sleptMs },
		{ "paused", m_Paused },
	};
	status["limits"] = {
		{ "maxCpus", limits.maxCpus },
		{ "maxMemoryBytes", limits.maxMemoryBytes },
		{ "nice", limits.nice },
		{ "pauseHours", limits.pauseFromHour >= 0 ? std::to_string(limits.pauseFromHour) + "-" + std::to_string(limits.pauseToHour) : "" },
	};
	status["made"] = m_Made;
	status["failed"] = m_Failed;
	status["evicted"] = m_Evicted;
	status["storedBytes"] = m_StoredBytes;
	status["maxStorageBytes"] = m_Config.maxStorageBytes;
	auto& recent = status["recent"] = nlohmann::json::array();
	for (auto it = m_Log.rbegin(); it != m_Log.rend(); ++it) {
		recent.push_back({ { "kind", KindName(it->kind) }, { "id", std::to_string(it->id) }, { "ok", it->ok },
			{ "ms", static_cast<int64_t>(it->milliseconds) }, { "message", it->message }, { "time", it->time } });
	}
	return status;
}
