#include "CDClientReloader.h"

#include <chrono>
#include <future>
#include <optional>
#include <set>

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "CDClientSnapshot.h"
#include "FdbSnapshot.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "dConfig.h"
#include "master/CDClientReload.h"

namespace {
	std::filesystem::path g_ClientFdb;
	std::filesystem::path g_ResServer;
	std::filesystem::path g_MigrationsDir;
	bool g_Enabled = false;

	std::optional<uint64_t> g_CurrentHash;
	std::optional<uint64_t> g_PreviousHash;
	std::string g_CurrentFdb;

	FdbSnapshot::Watcher g_Watcher;
	std::chrono::steady_clock::time_point g_NextPoll;

	std::future<CDClientSnapshot::Result> g_Job;
	FdbSnapshot::Stamp g_JobStamp;
	std::string g_JobWho;
	std::string g_Pending; // a request while a job runs; checked after it

	std::function<void(const CDClientReload&)> g_Broadcast;

	std::chrono::seconds PollInterval() {
		const auto seconds = GeneralUtils::TryParse<uint32_t>(Game::config->GetValue("cdclient_watch_seconds")).value_or(5);
		return std::chrono::seconds(seconds);
	}

	std::set<uint64_t> Keep() {
		std::set<uint64_t> keep;
		if (g_CurrentHash) keep.insert(*g_CurrentHash);
		if (g_PreviousHash) keep.insert(*g_PreviousHash);
		return keep;
	}

	void RemoveOld() {
		for (const auto& name : FdbSnapshot::RemoveOld(g_ResServer, Keep())) LOG("CDClient reload: removed the old %s", name.c_str());
	}

	void Start(const std::string& who) {
		g_JobStamp = FdbSnapshot::StampOf(g_ClientFdb);
		g_JobWho = who;
		const auto previous = g_CurrentFdb.empty() ? std::filesystem::path{} : g_ResServer / g_CurrentFdb;
		LOG("CDClient reload: checking %s (%s)", g_ClientFdb.string().c_str(), who.c_str());
		// The worker gets copies of everything and its own files and SQLite connection; it never logs
		g_Job = std::async(std::launch::async, CDClientSnapshot::Build, g_ClientFdb, g_ResServer, g_MigrationsDir, g_CurrentHash, previous);
	}

	void Finish(CDClientSnapshot::Result result) {
		// Taken either way, so a broken file isn't retried every poll; the next change to it is
		g_Watcher.Accept(g_JobStamp);
		if (!result.ok) {
			LOG("CDClient reload failed: %s", result.error.c_str());
			return;
		}
		if (result.unchanged) {
			LOG("CDClient reload: %s has not changed", g_ClientFdb.filename().string().c_str());
			return;
		}

		const auto sqlite = g_ResServer / result.current.sqlite;
		const auto fdb = g_ResServer / result.current.fdb;
		try {
			CDClientDatabase::Reconnect(sqlite.string());
			CDClientManager::Reload(fdb);
		} catch (const std::exception& e) {
			LOG("CDClient reload: master could not switch to %s: %s", sqlite.string().c_str(), e.what());
			return;
		}

		LOG("CDClient reload: switched to %s and %s (%s)", result.current.fdb.c_str(), result.current.sqlite.c_str(), g_JobWho.c_str());
		for (const auto& migration : result.migrations) LOG("CDClient reload: applied cdserver migration %s", migration.c_str());
		if (result.changes.empty()) LOG("CDClient reload: no table changed");
		for (const auto& change : result.changes) LOG("CDClient reload: %s", change.c_str());

		if (!FdbSnapshot::WriteCurrent(g_ResServer, result.current)) LOG("CDClient reload: could not write %s", FdbSnapshot::CURRENT_FILE);
		g_PreviousHash = g_CurrentHash;
		g_CurrentHash = result.hash;
		g_CurrentFdb = result.current.fdb;

		CDClientReload reload;
		reload.fdb = result.current.fdb;
		reload.sqlite = result.current.sqlite;
		if (g_Broadcast) g_Broadcast(reload);

		// Copies older than the last two: a server still on one of those switches before the next reload, and a file
		// still mapped (Windows won't remove it) is tried again next time
		RemoveOld();
	}
}

CDClientReloader::Files CDClientReloader::Init(const std::filesystem::path& clientFdb, const std::filesystem::path& resServer, const std::filesystem::path& migrationsDir) {
	g_ClientFdb = clientFdb;
	g_ResServer = resServer;
	g_MigrationsDir = migrationsDir;
	Files files{ resServer / FdbSnapshot::DEFAULT_SQLITE, {} };

	std::error_code code;
	if (!std::filesystem::is_regular_file(clientFdb, code)) {
		LOG("No loose %s: reading CDClient from CDServer.sqlite only, and not watching for changes", clientFdb.string().c_str());
		return files;
	}

	const auto stamp = FdbSnapshot::StampOf(clientFdb);
	std::string error;
	const auto hash = FdbSnapshot::MakeCopy(clientFdb, resServer, error);
	if (!hash) {
		LOG("Could not copy %s (%s): reading CDClient from CDServer.sqlite only", clientFdb.string().c_str(), error.c_str());
		return files;
	}

	FdbSnapshot::Current current{ FdbSnapshot::FdbName(*hash), FdbSnapshot::DEFAULT_SQLITE };
	const auto pointer = FdbSnapshot::ReadCurrent(resServer);
	if (pointer && pointer->fdb == current.fdb && std::filesystem::is_regular_file(resServer / pointer->sqlite, code)) {
		current = *pointer;
	} else if (pointer) {
		// The client's fdb changed while master was down: make its CDServer.sqlite now
		const auto sqlite = FdbSnapshot::SqliteName(*hash);
		if (!std::filesystem::is_regular_file(resServer / sqlite, code)) {
			LOG("%s changed since the last run; making %s. This may take a while", clientFdb.string().c_str(), sqlite.c_str());
			Game::logger->Flush();
			std::vector<std::string> migrations;
			if (!CDClientSnapshot::MakeSqlite(resServer / current.fdb, resServer / sqlite, migrationsDir, migrations, error)) {
				LOG("Could not make %s (%s); using CDServer.sqlite", sqlite.c_str(), error.c_str());
			}
		}
		if (std::filesystem::is_regular_file(resServer / sqlite, code)) current.sqlite = sqlite;
		g_PreviousHash = FdbSnapshot::ParseName(pointer->fdb);
	}
	// With no pointer file yet (the first run with copies), CDServer.sqlite is the one made from this fdb

	if (!FdbSnapshot::WriteCurrent(resServer, current)) LOG("Could not write %s", (resServer / FdbSnapshot::CURRENT_FILE).string().c_str());
	g_CurrentHash = *hash;
	g_CurrentFdb = current.fdb;
	g_Enabled = true;
	g_Watcher.Accept(stamp);
	RemoveOld();

	files.sqlite = resServer / current.sqlite;
	files.fdb = resServer / current.fdb;
	LOG("CDClient: using %s and %s (copied from %s)", current.fdb.c_str(), current.sqlite.c_str(), clientFdb.string().c_str());
	return files;
}

void CDClientReloader::SetBroadcast(std::function<void(const CDClientReload&)> broadcast) {
	g_Broadcast = std::move(broadcast);
}

void CDClientReloader::Request(const std::string& who) {
	if (!g_Enabled) {
		LOG("CDClient reload asked for by %s, but there is no loose cdclient.fdb to reload from", who.c_str());
		return;
	}
	if (g_Job.valid()) {
		g_Pending = who;
		return;
	}
	Start(who);
}

void CDClientReloader::Update() {
	if (!g_Enabled) return;

	if (g_Job.valid()) {
		if (g_Job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
		Finish(g_Job.get());
		if (!g_Pending.empty()) {
			Start(g_Pending);
			g_Pending.clear();
		}
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now < g_NextPoll) return;
	const auto interval = PollInterval();
	g_NextPoll = now + (interval.count() > 0 ? interval : std::chrono::seconds(60));
	if (interval.count() == 0) return;
	if (g_Watcher.Poll(FdbSnapshot::StampOf(g_ClientFdb))) Start("the file changed");
}

void CDClientReloader::Shutdown() {
	if (g_Job.valid()) g_Job.wait();
}
