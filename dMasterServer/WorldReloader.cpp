#include "WorldReloader.h"

#include <cctype>
#include <chrono>
#include <future>
#include <map>
#include <set>

#include "Game.h"
#include "GeneralUtils.h"
#include "InstanceManager.h"
#include "Logger.h"
#include "MigrationCoordinator.h"
#include "WorldFileWatch.h"
#include "dConfig.h"
#include "master/InstanceMigration.h"
#include "master/WorldFiles.h"

using namespace WorldFileWatch;

namespace {
	using Clock = std::chrono::steady_clock;
	// World reload migrations get IDs of their own, far from the worlds' (time-based) and the live update's ones
	constexpr uint32_t MIGRATION_ID_BASE = 0x80000000;
	constexpr auto PUBLISH_INTERVAL = std::chrono::seconds(1);

	struct HashResult {
		HashJob job;
		std::optional<uint64_t> hash;
		uint64_t size{};
	};

	Tracker g_Tracker;
	std::future<std::vector<HashResult>> g_Job;
	Clock::time_point g_NextPoll{};
	Clock::time_point g_NextPublish{};
	bool g_Dirty = true;
	uint32_t g_NextMigrationId = 0;
	std::map<InstanceKey, uint64_t> g_Tried;           // automatic reloads: the signature last tried per instance
	std::map<uint32_t, InstanceKey> g_Migrations;       // our migrations -> the instance being replaced
	std::map<uint32_t, std::string> g_ZoneMessages;     // the last reload of each zone, for the dashboard
	std::function<void(const WorldFilesStatus&)> g_Publisher;
	WorldFilesStatus g_Published;

	uint32_t WatchSeconds() {
		return GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("world_watch_seconds")).value_or(5);
	}

	bool Seamless() {
		return Game::config->GetValue("world_reload_seamless") == "1";
	}

	std::set<uint32_t> KeepZones() {
		std::set<uint32_t> zones;
		for (auto part : GeneralUtils::SplitString(Game::config->GetValue("prestart_worlds", "0,1000"), ',')) {
			std::erase_if(part, [](const char c) { return std::isspace(static_cast<unsigned char>(c)); });
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(part)) zones.insert(*zone);
		}
		return zones;
	}

	std::string FileName(const std::string& path) {
		const auto slash = path.find_last_of("/\\");
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}

	// The worker gets copies of the paths; it only reads the files and never logs or touches shared state
	std::vector<HashResult> HashFiles(std::vector<HashJob> jobs) {
		std::vector<HashResult> results;
		results.reserve(jobs.size());
		for (auto& job : jobs) {
			HashResult result;
			std::error_code code;
			const auto size = std::filesystem::file_size(job.path, code);
			if (!code) {
				result.size = static_cast<uint64_t>(size);
				result.hash = FdbSnapshot::HashFile(job.path);
			}
			result.job = std::move(job);
			results.push_back(std::move(result));
		}
		return results;
	}

	std::vector<InstanceMigration::InstanceView> Views() {
		std::vector<InstanceMigration::InstanceView> views;
		for (const auto& instance : Game::im->GetInstances()) {
			if (instance) views.push_back(instance->View());
		}
		return views;
	}

	void SetZoneMessage(uint32_t zone, const std::string& message) {
		g_ZoneMessages[zone] = message;
		g_Dirty = true;
	}

	/**
	 * Replaces what Choose picked. warnSeconds: how long players are warned; requesterId: the GM told how each move
	 * goes. Returns how many instances are being replaced or stopped.
	 */
	uint32_t Apply(const std::vector<Choice>& choices, uint16_t warnSeconds, LWOOBJID requesterId, const std::string& by) {
		uint32_t acted = 0;
		std::map<uint32_t, std::pair<uint32_t, uint32_t>> perZone; // zone -> (acted, skipped)
		for (const auto& choice : choices) {
			const auto& view = choice.view;
			auto& counts = perZone[view.zoneId];
			if (choice.action != eAction::SKIP) {
				// On the old files: nobody new goes there (a request starts an instance on the files on disk now)
				Game::im->MarkOutdated([&view](const Instance& instance) {
					return instance.GetMapID() == view.zoneId && instance.GetInstanceID() == view.instanceId;
				});
			}
			switch (choice.action) {
			case eAction::SKIP:
				LOG("World reload (%s): zone %u instance %u left alone: %s", by.c_str(), view.zoneId, view.instanceId, choice.reason.c_str());
				counts.second++;
				continue;
			case eAction::KEEP_UNTIL_EMPTY:
				LOG("World reload (%s): property zone %u clone %u instance %u keeps its %d player(s); it takes nobody new and stops once empty",
					by.c_str(), view.zoneId, view.cloneId, view.instanceId, view.players);
				counts.first++;
				acted++;
				continue;
			case eAction::STOP:
			case eAction::START_THEN_STOP: {
				const auto& instance = Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(view.zoneId), static_cast<LWOINSTANCEID>(view.instanceId));
				if (!instance) continue;
				if (choice.action == eAction::START_THEN_STOP) {
					// Started first: the stopped one takes nobody new meanwhile
					instance->SetIsDraining(true);
					const auto& started = Game::im->StartNewInstance(static_cast<LWOMAPID>(view.zoneId), 0);
					if (started) LOG("World reload (%s): started instance %u of zone %u", by.c_str(), started->GetInstanceID(), view.zoneId);
				}
				// Look it up again: starting one grew the instance list
				const auto& stopping = Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(view.zoneId), static_cast<LWOINSTANCEID>(view.instanceId));
				if (!stopping) continue;
				LOG("World reload (%s): zone %u instance %u is empty; stopping it", by.c_str(), view.zoneId, view.instanceId);
				stopping->Shutdown();
				counts.first++;
				acted++;
				continue;
			}
			case eAction::REPLACE: {
				InstanceMigrationRequest request;
				request.requestId = MIGRATION_ID_BASE | (++g_NextMigrationId & 0x3FFFFFFF);
				request.kind = InstanceMigration::eKind::REPLACE;
				request.zoneId = view.zoneId;
				request.sourceInstance = view.instanceId;
				request.warnSeconds = warnSeconds;
				request.shutdownSource = true;
				request.seamless = Seamless();
				request.requesterId = requesterId;
				request.requestedBy = ("world reload (" + by + ")").substr(0, InstanceMigrationRequest::MAX_BY);
				MigrationCoordinator::Options options;
				// Private instances and activity zones are moved too (properties never get here: KEEP_UNTIL_EMPTY)
				options.liveUpdate = true;
				g_Migrations[request.requestId] = { view.zoneId, view.instanceId };
				const auto refusal = MigrationCoordinator::Start(request, options);
				if (refusal != InstanceMigration::eRefusal::NONE) {
					g_Migrations.erase(request.requestId);
					LOG("World reload (%s): zone %u instance %u could not be replaced: %s", by.c_str(), view.zoneId, view.instanceId, InstanceMigration::Describe(refusal));
					counts.second++;
					continue;
				}
				counts.first++;
				acted++;
				continue;
			}
			}
		}
		for (const auto& [zone, counts] : perZone) {
			std::string message = "Reload (" + by + "): " + std::to_string(counts.first) + " instance(s) replaced, stopped or left to empty";
			if (counts.second) message += ", " + std::to_string(counts.second) + " left alone";
			SetZoneMessage(zone, message);
		}
		return acted;
	}

	// Instances that loaded a file that changed since, each tried once per version of its files
	void ReloadStale() {
		std::set<InstanceKey> wanted;
		for (const auto& key : g_Tracker.Stale()) {
			const auto tried = g_Tried.find(key);
			if (tried != g_Tried.end() && tried->second == g_Tracker.Signature(key)) continue;
			wanted.insert(key);
		}
		if (wanted.empty()) return;
		auto choices = Choose(Views(), [&wanted](const InstanceMigration::InstanceView& view) {
			return wanted.contains({ view.zoneId, view.instanceId });
		}, KeepZones());
		// Instances still starting (or already moving) are tried again at the next poll
		std::erase_if(choices, [](const Choice& choice) { return choice.action == eAction::SKIP; });
		if (choices.empty()) return;
		for (const auto& choice : choices) {
			const InstanceKey key{ choice.view.zoneId, choice.view.instanceId };
			g_Tried[key] = g_Tracker.Signature(key);
			std::string files;
			for (const auto& path : g_Tracker.ChangedFiles(key)) files += (files.empty() ? "" : ", ") + FileName(path);
			LOG("World reload: zone %u instance %u loaded files that changed on disk (%s): %s", choice.view.zoneId, choice.view.instanceId,
				files.c_str(), ActionName(choice.action));
		}
		Apply(choices, WorldReloadRequest::DEFAULT_WARN_SECONDS, LWOOBJID_EMPTY, "files changed");
	}

	WorldFilesStatus BuildStatus() {
		WorldFilesStatus status;
		const auto seconds = WatchSeconds();
		status.watching = seconds > 0;
		status.watchSeconds = static_cast<uint16_t>(std::min<uint32_t>(seconds, UINT16_MAX));
		status.seamless = Seamless();
		for (const auto zoneId : g_Tracker.Zones()) {
			WorldFilesStatus::Zone zone;
			zone.zoneId = zoneId;
			for (const auto& path : g_Tracker.FilesOf(zoneId)) {
				const auto it = g_Tracker.Files().find(path);
				if (it == g_Tracker.Files().end()) continue;
				const auto& state = it->second;
				WorldFilesStatus::File file;
				file.disk.kind = state.kind;
				file.disk.packed = state.packed;
				file.disk.path = path;
				file.hashed = state.hash.has_value();
				file.disk.size = state.hash ? state.size : state.reportedSize;
				file.disk.hash = state.hash ? *state.hash : state.reportedHash;
				file.missing = state.missing;
				file.changed = g_Tracker.IsChanged(zoneId, path);
				zone.files.push_back(std::move(file));
			}
			for (const auto& instance : Game::im->GetInstances()) {
				if (!instance || instance->GetMapID() != zoneId) continue;
				WorldFilesStatus::Instance row;
				row.instanceId = instance->GetInstanceID();
				row.cloneId = instance->GetCloneID();
				row.players = instance->GetCurrentClientCount();
				row.stale = g_Tracker.IsStale({ zoneId, row.instanceId });
				row.reloading = instance->GetIsShuttingDown() || instance->GetIsDraining();
				row.outdated = instance->GetIsOutdated();
				zone.instances.push_back(row);
			}
			if (const auto message = g_ZoneMessages.find(zoneId); message != g_ZoneMessages.end()) zone.message = message->second;
			status.zones.push_back(std::move(zone));
		}
		return status;
	}

	void Publish(bool force) {
		if (!g_Publisher) return;
		const auto now = Clock::now();
		if (!force && (!g_Dirty || now < g_NextPublish)) return;
		g_Dirty = false;
		g_NextPublish = now + PUBLISH_INTERVAL;
		auto status = BuildStatus();
		const bool same = status.watching == g_Published.watching && status.watchSeconds == g_Published.watchSeconds &&
			status.seamless == g_Published.seamless && status.zones == g_Published.zones;
		if (same && !force) return;
		g_Published = status;
		g_Publisher(status);
	}
}

void WorldReloader::SetPublisher(std::function<void(const WorldFilesStatus&)> publisher) {
	g_Publisher = std::move(publisher);
}

void WorldReloader::HandleReport(const SystemAddress& from, const WorldFilesReport& report) {
	const auto& instance = Game::im->GetInstanceBySysAddr(from);
	if (!instance) {
		LOG("Ignoring a world file list from a server that is not a world");
		return;
	}
	uint32_t watched = 0;
	for (const auto& file : report.files) watched += file.packed ? 0 : 1;
	LOG("Zone %u instance %u loaded %zu zone file(s) (%u loose, watched)", instance->GetMapID(), instance->GetInstanceID(), report.files.size(), watched);
	g_Tracker.Report(instance->GetMapID(), instance->GetInstanceID(), instance->GetCloneID(), report.files);
	g_Dirty = true;
}

void WorldReloader::HandleRequest(const WorldReloadRequest& request, const std::string& who) {
	if (request.zoneId == 0) {
		LOG("World reload asked for by %s without a zone; nothing to do", who.c_str());
		return;
	}
	const auto zone = request.zoneId;
	const auto choices = Choose(Views(), [zone](const InstanceMigration::InstanceView& view) { return view.zoneId == zone; }, KeepZones());
	if (choices.empty()) {
		LOG("World reload of zone %u asked for by %s: no instance of it runs", zone, who.c_str());
		SetZoneMessage(zone, "Reload (" + who + "): no instance was running");
		return;
	}
	LOG("World reload of zone %u asked for by %s: %zu instance(s)", zone, who.c_str(), choices.size());
	for (const auto& choice : choices) {
		// A replaced instance is not tried again automatically for the files on disk now
		g_Tried[{ choice.view.zoneId, choice.view.instanceId }] = g_Tracker.Signature({ choice.view.zoneId, choice.view.instanceId });
	}
	Apply(choices, std::min(request.warnSeconds, InstanceMigrationRequest::MAX_WARN_SECONDS), request.requesterId, who);
}

void WorldReloader::OnMigrationStatus(const MigrationStatus& status) {
	const auto it = g_Migrations.find(status.migrationId);
	if (it == g_Migrations.end()) return;
	if (!status.Finished()) {
		g_Dirty = true;
		return;
	}
	const auto key = it->second;
	g_Migrations.erase(it);
	std::string message = "Instance " + std::to_string(key.instance);
	if (status.state == InstanceMigration::eState::DONE) {
		message += " replaced by " + std::to_string(status.targetInstance) + " (" + std::to_string(status.moved) + " player(s) moved)";
	} else {
		message += " was not replaced: " + (status.message.empty() ? std::string("the move failed") : status.message);
		// Tried again once its files change again, or when asked
	}
	SetZoneMessage(key.zone, message);
}

void WorldReloader::OnInstanceGone(const Instance& instance) {
	const InstanceKey key{ instance.GetMapID(), instance.GetInstanceID() };
	g_Tracker.Forget(key.zone, key.instance);
	g_Tried.erase(key);
	if (g_Tracker.Zones().count(key.zone) == 0) g_ZoneMessages.erase(key.zone);
	g_Dirty = true;
}

void WorldReloader::Republish() {
	g_Dirty = true;
	Publish(true);
}

void WorldReloader::Update() {
	if (g_Job.valid()) {
		if (g_Job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return Publish(false);
		for (const auto& result : g_Job.get()) {
			const bool changed = g_Tracker.Hashed(result.job, result.hash, result.size);
			if (changed) LOG("World reload: %s changed on disk", result.job.path.c_str());
			else if (!result.hash) LOG("World reload: could not read %s", result.job.path.c_str());
		}
		g_Dirty = true;
		ReloadStale();
	}

	const auto now = Clock::now();
	if (now >= g_NextPoll) {
		const auto seconds = WatchSeconds();
		g_NextPoll = now + std::chrono::seconds(seconds > 0 ? seconds : 60);
		if (seconds > 0) {
			// Worlds that reported after the last change are caught here too
			ReloadStale();
			auto due = g_Tracker.Poll([](const std::string& path) { return FdbSnapshot::StampOf(path); });
			if (!due.empty()) g_Job = std::async(std::launch::async, HashFiles, std::move(due));
		}
		// Player counts change without telling us; the status is compared before it is sent
		g_Dirty = true;
	}
	Publish(false);
}

void WorldReloader::Shutdown() {
	if (g_Job.valid()) g_Job.wait();
}
