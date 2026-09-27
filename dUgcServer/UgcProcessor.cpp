#include "UgcProcessor.h"

#include "CDClientDatabase.h"
#include "Database.h"
#include "Logger.h"
#include "UgcCdClient.h"
#include "json.hpp"

namespace {
	constexpr size_t MAX_ERROR_LENGTH = 1000; // process_error holds 1024
	constexpr size_t LOG_LENGTH = 50;
	constexpr auto EVICTION_INTERVAL = std::chrono::minutes(5);
	constexpr auto RECENT_ANSWER_TIME = std::chrono::seconds(10);

	const char* KindName(UgcStorage::Kind kind) {
		return kind == UgcStorage::Kind::MODEL ? "model" : "modular";
	}

	int64_t UnixNow() {
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}

UgcProcessor::UgcProcessor(Config config, UgcStorage& storage, UgcBricks::BrickLibrary& library, UgcJobs::Settings settings)
	: m_Config(config), m_Storage(storage), m_Library(library), m_Settings(std::move(settings)) {}

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
	while (true) {
		Job job;
		{
			std::unique_lock lock(m_Mutex);
			m_Wake.wait(lock, [this] { return m_Stopping || !m_Jobs.empty(); });
			if (m_Stopping) return;
			job = std::move(m_Jobs.front());
			m_Jobs.pop_front();
			m_Active++;
		}

		const auto start = std::chrono::steady_clock::now();
		Done done{ job.kind, job.id, job.attempts };
		try {
			done.outcome = job.kind == Kind::MODEL
				? UgcJobs::ProcessModel(job.blob, m_Library, m_Settings)
				: UgcJobs::ProcessModular(job.modular, m_Library.GetResPath(), m_Settings);
		} catch (const std::exception& ex) {
			done.outcome.ok = false;
			done.outcome.error = std::string("crashed: ") + ex.what();
		}
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

		std::lock_guard lock(m_Mutex);
		m_Active--;
		m_Done.push_back(std::move(done));
	}
}

void UgcProcessor::Poll() {
	size_t queued = 0, active = 0;
	{
		std::lock_guard lock(m_Mutex);
		queued = m_Jobs.size();
		active = m_Active;
	}
	// Enough to keep every worker busy until the next poll
	const size_t wanted = std::max<size_t>(m_Threads.size() * 2, m_Config.pollBatch);
	if (queued + active >= wanted) return;
	const auto limit = static_cast<uint32_t>(wanted - queued - active + m_InFlight.size());

	std::vector<Job> jobs;
	for (auto& model : Database::Get()->GetUgcModelsToProcess(limit)) {
		if (m_InFlight.contains({ Kind::MODEL, model.id })) continue;
		jobs.push_back(Job{ Kind::MODEL, model.id, model.attempts, std::move(model.lxfml) });
	}
	for (auto& build : Database::Get()->GetModularBuildsToProcess(limit)) {
		if (m_InFlight.contains({ Kind::MODULAR, build.id })) continue;
		Job job{ Kind::MODULAR, build.id, build.attempts };
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
	status["workers"] = m_Threads.size();
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
