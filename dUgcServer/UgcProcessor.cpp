#include "UgcProcessor.h"

#include "CDClientDatabase.h"
#include "Database.h"
#include "Logger.h"
#include "UgcBricks.h"
#include "UgcCdClient.h"
#include "UgcKeys.h"
#include "UgcThrottle.h"
#include "json.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
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
		if (job.preview) {
			// A preview: the icon goes back to whoever asked, nothing is stored or recorded
			HTTPReply reply;
			try {
				UgcJobs::Outcome outcome{ false, "cancelled" };
				if (!job.preview.Cancelled() && job.kind == Kind::MODULAR) {
					outcome = UgcJobs::ProcessModular(job.modular, m_Library.GetResPath(), settings);
				} else if (!job.preview.Cancelled()) {
					// A player model's icon from its stored .nif
					const auto path = m_Storage.File(Kind::MODEL, job.id, "model.nif");
					const auto nif = path ? UgcBricks::ReadFile(*path) : std::nullopt;
					auto options = settings.icon;
					UgcIconParams::Apply(options, job.iconValues);
					outcome.ok = nif && UgcJobs::IconFromNif(*nif, options, outcome.files, outcome.error);
					if (!nif) outcome.error = "the model has no stored .nif yet";
				}
				if (outcome.ok && outcome.files.contains("icon.png")) {
					reply.status = eHTTPStatusCode::OK;
					reply.contentType = eContentType::IMAGE_PNG;
					reply.message = std::move(outcome.files["icon.png"]);
					reply.headers.push_back("Cache-Control: no-store");
				} else {
					reply.status = eHTTPStatusCode::UNPROCESSABLE_ENTITY;
					reply.contentType = eContentType::TEXT_PLAIN;
					reply.message = outcome.error;
				}
			} catch (const std::exception& ex) {
				reply.status = eHTTPStatusCode::INTERNAL_SERVER_ERROR;
				reply.contentType = eContentType::TEXT_PLAIN;
				reply.message = ex.what();
			}
			job.preview.Send(std::move(reply));
			{
				std::lock_guard lock(m_Mutex);
				m_Active--;
				m_MemoryInUse -= std::min(m_MemoryInUse, job.memory);
			}
			m_Wake.notify_all();
			continue;
		}
		const auto start = std::chrono::steady_clock::now();
		Done done{ job.kind, job.id, job.attempts };
		done.iconOnly = job.iconOnly;
		try {
			if (job.iconOnly) {
				// Only the icon, from the .nif made before
				const auto path = m_Storage.File(Kind::MODEL, job.id, "model.nif");
				const auto nif = path ? UgcBricks::ReadFile(*path) : std::nullopt;
				auto options = settings.icon;
				UgcIconParams::Apply(options, job.iconValues);
				done.outcome.ok = nif && UgcJobs::IconFromNif(*nif, options, done.outcome.files, done.outcome.error);
				if (!nif) done.outcome.error = "no stored .nif";
			} else {
				done.outcome = job.kind == Kind::MODEL
					? UgcJobs::ProcessModel(job.blob, m_Library, settings, static_cast<uint64_t>(job.id), job.iconValues)
					: UgcJobs::ProcessModular(job.modular, m_Library.GetResPath(), settings);
			}
		} catch (const std::exception& ex) {
			done.outcome.ok = false;
			done.outcome.error = std::string("crashed: ") + ex.what();
		}
		job.blob.clear();
		job.blob.shrink_to_fit();
		if (done.outcome.ok) {
			std::string error;
			const auto bytes = job.iconOnly ? m_Storage.Update(job.kind, job.id, done.outcome.files, error) : m_Storage.Write(job.kind, job.id, done.outcome.files, error);
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
		job.iconValues = IconValues(UgcIconParams::ModelKind(), UgcIconParams::ModelTarget(model.id));
		job.memory = UgcJobs::EstimateMemory(job.parts, settings);
		jobs.push_back(std::move(job));
	}
	// Cars and rockets: one icon per combination of modules, shared by every build of it
	for (auto& build : Database::Get()->GetModularBuildsToProcess(limit)) {
		if (m_InFlight.contains({ Kind::MODULAR, build.id })) continue;
		const auto key = UgcModularKey::Normalize(build.modules);
		if (key.empty()) {
			Record(Done{ Kind::MODULAR, build.id, build.attempts, UgcJobs::Outcome{ false, "no modules in \"" + build.modules + "\"" } });
			continue;
		}
		const auto combo = UgcModularKey::StorageId(key);
		m_ComboOf[build.id] = combo;
		// Made already for another build of the same modules
		if (!m_ComboJobs.contains(combo) && m_Storage.File(Kind::MODULAR, combo, "icon.png")) {
			UgcJobs::Outcome reused{ true };
			reused.note = "the same modules as a build made before (" + key + ")";
			m_Reused++;
			Record(Done{ Kind::MODULAR, build.id, build.attempts, std::move(reused) });
			continue;
		}
		m_InFlight.insert({ Kind::MODULAR, build.id });
		m_ComboRows[combo].emplace_back(build.id, build.attempts);
		if (m_ComboJobs.contains(combo)) continue;
		Job job{ Kind::MODULAR, combo, build.attempts };
		job.memory = UgcJobs::EstimateMemory(64, settings);
		std::string error;
		if (!UgcCdClient::GatherModular(build.modules, job.modular, error)) {
			// Nothing a worker could do: record it right away
			m_InFlight.erase({ Kind::MODULAR, build.id });
			m_ComboRows.erase(combo);
			Record(Done{ Kind::MODULAR, build.id, build.attempts, UgcJobs::Outcome{ false, error } });
			continue;
		}
		job.modular.key = key;
		job.modular.iconValues = IconValues(UgcIconParams::BuildKind(job.modular.buildType), UgcIconParams::CombinationTarget(key));
		m_ComboJobs.insert(combo);
		jobs.push_back(std::move(job));
	}
	if (jobs.empty()) return;
	{
		std::lock_guard lock(m_Mutex);
		for (auto& job : jobs) {
			if (job.kind == Kind::MODEL) m_InFlight.insert({ job.kind, job.id });
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
		if (done.kind == Kind::MODEL) {
			m_InFlight.erase({ done.kind, done.id });
			if (!done.iconOnly) {
				Record(done);
				continue;
			}
			m_Log.push_back({ Kind::MODEL, done.id, done.outcome.ok, done.milliseconds, done.outcome.ok ? "icon drawn again" : done.outcome.error, UnixNow() });
			while (m_Log.size() > LOG_LENGTH) m_Log.pop_front();
			continue;
		}
		// A combination: every build waiting for it gets its outcome (none when only its icon was drawn again)
		m_ComboJobs.erase(done.id);
		if (done.outcome.ok) m_StoredBytes += done.bytes;
		auto rows = std::move(m_ComboRows[done.id]);
		m_ComboRows.erase(done.id);
		for (const auto& [row, attempts] : rows) {
			m_InFlight.erase({ Kind::MODULAR, row });
			Done forRow = done;
			forRow.id = row;
			forRow.attempts = attempts;
			forRow.bytes = 0;
			Record(forRow);
		}
		if (rows.empty()) {
			m_Log.push_back({ Kind::MODULAR, done.id, done.outcome.ok, done.milliseconds, done.outcome.ok ? "icon drawn again" : done.outcome.error, UnixNow() });
			while (m_Log.size() > LOG_LENGTH) m_Log.pop_front();
		}
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

LWOOBJID UgcProcessor::StorageId(Kind kind, LWOOBJID id) {
	if (kind == Kind::MODEL) return id;
	if (const auto it = m_ComboOf.find(id); it != m_ComboOf.end()) return it->second;
	const auto info = Database::Get()->GetModularBuildProcessInfo(id);
	const auto key = info ? UgcModularKey::Normalize(info->details) : std::string();
	const LWOOBJID combo = key.empty() ? 0 : UgcModularKey::StorageId(key);
	if (combo != 0) m_ComboOf[id] = combo;
	return combo;
}

UgcProcessor::Availability UgcProcessor::Request(Kind kind, LWOOBJID id) {
	if (m_InFlight.contains({ kind, id })) return Availability::QUEUED;
	const auto storageId = StorageId(kind, id);
	if (kind == Kind::MODULAR && m_ComboJobs.contains(storageId)) return Availability::QUEUED;
	if (storageId != 0 && m_Storage.File(kind, storageId, "icon.png")) {
		m_Storage.Touch(kind, storageId);
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
		// Someone wants it now: no need to wait out the quiet period after its save
		if (kind == Kind::MODEL) Database::Get()->ExpediteUgcModel(id);
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
	status["reused"] = m_Reused;
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

UgcIconParams::Values UgcProcessor::IconValues(const std::string& kind, const std::string& itemTarget) {
	UgcIconParams::Values values;
	if (const auto preset = Database::Get()->GetUgcIconSettings(UgcIconParams::KindTarget(kind))) values = UgcIconParams::Parse(*preset);
	if (const auto own = Database::Get()->GetUgcIconSettings(itemTarget)) {
		for (const auto& [key, value] : UgcIconParams::Parse(*own)) values[key] = value;
	}
	return values;
}

bool UgcProcessor::QueuePreview(Kind kind, LWOOBJID id, const std::string& modules, const UgcIconParams::Values& values, DeferredReply reply, std::string& error) {
	Job job{ kind, id, 0 };
	if (kind == Kind::MODULAR) {
		if (!UgcCdClient::GatherModular(modules, job.modular, error)) return false;
		job.modular.key = UgcModularKey::Normalize(modules);
		job.modular.iconValues = values;
	} else {
		if (!m_Storage.File(Kind::MODEL, id, "model.nif")) {
			error = "the model has no stored .nif yet";
			return false;
		}
		job.iconValues = values;
	}
	job.preview = std::move(reply);
	{
		std::lock_guard lock(m_Mutex);
		job.memory = UgcJobs::EstimateMemory(64, m_Settings);
		m_Jobs.push_front(std::move(job));
	}
	m_Wake.notify_all();
	return true;
}

size_t UgcProcessor::RegenerateIcons(const std::string& kind) {
	std::vector<Job> jobs;
	UgcJobs::Settings settings;
	{
		std::lock_guard lock(m_Mutex);
		settings = m_Settings;
	}
	const bool models = kind == UgcIconParams::ModelKind();
	for (const auto& entry : m_Storage.List()) {
		if (models) {
			if (entry.kind != Kind::MODEL || m_InFlight.contains({ Kind::MODEL, entry.id }) || !m_Storage.File(Kind::MODEL, entry.id, "model.nif")) continue;
			Job job{ Kind::MODEL, entry.id, 0 };
			job.iconOnly = true;
			job.iconValues = IconValues(kind, UgcIconParams::ModelTarget(entry.id));
			job.memory = UgcJobs::EstimateMemory(64, settings);
			m_InFlight.insert({ Kind::MODEL, entry.id });
			jobs.push_back(std::move(job));
			continue;
		}
		if (entry.kind != Kind::MODULAR || m_ComboJobs.contains(entry.id)) continue;
		const auto path = m_Storage.File(Kind::MODULAR, entry.id, "combo.json");
		const auto text = path ? UgcBricks::ReadFile(*path) : std::nullopt;
		const auto combo = text ? nlohmann::json::parse(*text, nullptr, false) : nlohmann::json();
		if (!combo.is_object() || UgcIconParams::BuildKind(combo.value("buildType", -1)) != kind) continue;
		const auto key = combo.value("key", std::string());
		Job job{ Kind::MODULAR, entry.id, 0 };
		std::string error;
		// The key's LOTs written as an ldf_config ("4713-4714" -> "4713+4714")
		std::string modules = key;
		std::replace(modules.begin(), modules.end(), '-', '+');
		if (key.empty() || !UgcCdClient::GatherModular(modules, job.modular, error)) continue;
		job.modular.key = key;
		job.modular.iconValues = IconValues(kind, UgcIconParams::CombinationTarget(key));
		job.memory = UgcJobs::EstimateMemory(64, settings);
		m_ComboJobs.insert(entry.id);
		jobs.push_back(std::move(job));
	}
	const auto count = jobs.size();
	{
		std::lock_guard lock(m_Mutex);
		for (auto& job : jobs) m_Jobs.push_back(std::move(job));
	}
	m_Wake.notify_all();
	return count;
}

UgcProcessor::DeleteResult UgcProcessor::Delete(const DeleteRequest& request) {
	DeleteResult result;
	const auto now = std::filesystem::file_time_type::clock::now();
	const auto days = [](int64_t count) { return std::chrono::duration_cast<std::filesystem::file_time_type::duration>(std::chrono::hours(24 * count)); };

	// What to delete: the files' ids, and the rows they came from (when they're known)
	std::map<LWOOBJID, std::vector<LWOOBJID>> targets; // storage id -> rows
	if (request.all) {
		for (const auto& entry : m_Storage.List()) {
			if (entry.kind == request.kind) targets[entry.id];
		}
	}
	for (const auto id : request.ids) {
		const auto storageId = StorageId(request.kind, id);
		if (storageId != 0) targets[storageId].push_back(id);
	}

	std::vector<LWOOBJID> deletedRows;
	for (const auto& [storageId, rows] : targets) {
		const bool busy = request.kind == Kind::MODEL ? m_InFlight.contains({ Kind::MODEL, storageId }) : m_ComboJobs.contains(storageId);
		if (busy) {
			result.busy++;
			continue;
		}
		const auto folder = m_Storage.Folder(request.kind, storageId);
		std::error_code error;
		if (!std::filesystem::exists(folder, error)) {
			deletedRows.insert(deletedRows.end(), rows.begin(), rows.end());
			continue;
		}
		if (request.unusedDays > 0) {
			const auto used = std::filesystem::last_write_time(folder, error);
			if (!error && now - used < days(request.unusedDays)) continue;
		}
		if (request.olderThanDays > 0) {
			const auto made = std::filesystem::last_write_time(folder / "icon.png", error);
			if (!error && now - made < days(request.olderThanDays)) continue;
		}
		uint64_t bytes = 0;
		for (const auto& file : std::filesystem::directory_iterator(folder, error)) bytes += file.is_regular_file(error) ? file.file_size(error) : 0;
		m_Storage.Remove(request.kind, storageId);
		result.deleted++;
		result.bytes += bytes;
		m_StoredBytes -= std::min(m_StoredBytes, bytes);
		m_Recent.clear();
		deletedRows.insert(deletedRows.end(), rows.begin(), rows.end());
		if (request.kind == Kind::MODEL) deletedRows.push_back(storageId);
	}
	std::sort(deletedRows.begin(), deletedRows.end());
	deletedRows.erase(std::unique(deletedRows.begin(), deletedRows.end()), deletedRows.end());

	// What happens to the rows
	if (request.after == eAfterDelete::NOW) {
		if (request.all) {
			if (request.kind == Kind::MODEL) Database::Get()->ResetUgcModelProcessing(std::nullopt, false);
			else Database::Get()->ResetModularBuildProcessing(std::nullopt, false);
		} else {
			for (const auto row : deletedRows) {
				if (request.kind == Kind::MODEL) Database::Get()->ResetUgcModelProcessing(row, false);
				else Database::Get()->ResetModularBuildProcessing(row, false);
			}
		}
		m_NextPoll = std::chrono::steady_clock::now();
		result.notes.push_back("They're queued to be made again now.");
	} else if (request.after == eAfterDelete::GONE) {
		for (const auto row : deletedRows) {
			if (request.kind == Kind::MODEL) Database::Get()->SetUgcModelProcessed(row, IUgc::eProcessState::FAILED, m_Config.maxAttempts, "Deleted from the dashboard", false);
			else Database::Get()->SetModularBuildProcessed(row, IUgc::eProcessState::FAILED, m_Config.maxAttempts, "Deleted from the dashboard");
		}
		result.notes.push_back("They're marked deleted and won't be made again unless made again from the dashboard.");
	} else {
		result.notes.push_back("They'll be made again when a game client asks for them.");
	}
	if (request.kind == Kind::MODULAR && result.deleted > 0) {
		result.notes.push_back("Car and rocket icons are shared by every build of the same modules: the other builds' icons are made again when asked for.");
	}
	if (result.busy > 0) result.notes.push_back(std::to_string(result.busy) + " being made right now were left alone.");
	return result;
}
