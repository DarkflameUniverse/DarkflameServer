#include "LiveUpdateCoordinator.h"

#include <cctype>
#include <ctime>
#include <filesystem>
#include <map>
#include <set>

#include "BinaryPathFinder.h"
#include "CDActivitiesTable.h"
#include "CDClientManager.h"
#include "Database.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "InstanceManager.h"
#include "Logger.h"
#include "MigrationCoordinator.h"
#include "MigrationRunner.h"
#include "dConfig.h"

using namespace LiveUpdate;

namespace {
	// How often the machine runs (master runs at 30 frames a second; nothing here needs that)
	constexpr auto TICK_INTERVAL = std::chrono::milliseconds(250);
	// Live update migrations get IDs of their own, far from the worlds' (time-based) ones
	constexpr uint32_t MIGRATION_ID_BASE = 0xC0000000;

	LiveUpdateCoordinator::Hooks g_Hooks;
	Machine g_Machine;
	LiveUpdateStatus g_Status; // the last one sent
	uint32_t g_UpdateId = 0;
	uint32_t g_NextMigrationId = 0;
	Clock::time_point g_NextTick{};
	// The instances running when the update started: marked outdated once the database is up to date
	std::set<std::pair<uint32_t, uint32_t>> g_OldInstances;
	bool g_OldMarked = false;
	void MarkOldInstances();
	std::map<size_t, std::pair<eUnitState, std::string>> g_Logged; // unit index -> what was logged last
	ePhase g_LoggedPhase = ePhase::IDLE;

	int64_t Now() {
		return static_cast<int64_t>(std::time(nullptr));
	}

	template<typename T>
	T Setting(const std::string& key, T fallback) {
		return GeneralUtils::TryParse<T>(Game::config->GetValue(key)).value_or(fallback);
	}

	std::set<uint32_t> KeepZones() {
		std::set<uint32_t> zones;
		for (auto part : GeneralUtils::SplitString(Game::config->GetValue("prestart_worlds", "0,1000"), ',')) {
			std::erase_if(part, [](const char c) { return std::isspace(static_cast<unsigned char>(c)); });
			if (const auto zone = GeneralUtils::TryParse<uint32_t>(part)) zones.insert(*zone);
		}
		zones.insert(0);
		return zones;
	}

	Settings ReadSettings(int32_t warnSeconds) {
		Settings settings;
		settings.warnSeconds = warnSeconds >= 0 ? static_cast<uint16_t>(warnSeconds) :
			std::min<uint16_t>(Setting<uint16_t>("live_update_warn_seconds", 10), InstanceMigrationRequest::MAX_WARN_SECONDS);
		settings.parallelWorlds = std::max<uint32_t>(Setting<uint32_t>("live_update_parallel_worlds", 4), 1);
		settings.serviceTimeout = std::chrono::seconds(std::max<uint32_t>(Setting<uint32_t>("live_update_service_timeout", 30), 10));
		settings.ugcDrainTimeout = std::chrono::seconds(Setting<uint32_t>("live_update_ugc_drain_timeout", 300));
		settings.charSelectWait = std::chrono::seconds(Setting<uint32_t>("live_update_char_select_wait", 60));
		settings.activityWait = std::chrono::seconds(Setting<uint32_t>("live_update_activity_wait", 1800));
		settings.playerWaitSeconds = std::min<uint16_t>(Setting<uint16_t>("live_update_player_wait", 30), MigratePlayersOrder::MAX_MAX_WAIT_SECONDS);
		settings.propertyBuildWaitSeconds = std::min<uint16_t>(Setting<uint16_t>("live_update_property_build_wait", 60), MigratePrepare::MAX_WAIT_SECONDS);
		settings.runMigrations = Game::config->GetValue("live_update_run_migrations") != "0";
		settings.keepZones = KeepZones();
		return settings;
	}

	// Races, minigames and other activities run in their own zones with state that only lives in that world
	bool IsActivityZone(uint32_t zone) {
		static std::map<uint32_t, bool> cache;
		if (const auto it = cache.find(zone); it != cache.end()) return it->second;
		auto* activities = CDClientManager::GetTable<CDActivitiesTable>();
		const bool activity = activities && !activities->Query([zone](const CDActivities& row) { return row.instanceMapID == zone; }).empty();
		return cache[zone] = activity;
	}

	Observed Observe() {
		Observed observed;
		observed.ugc = g_Hooks.service(eService::UGC);
		observed.auth = g_Hooks.service(eService::AUTH);
		observed.chat = g_Hooks.service(eService::CHAT);
		observed.dashboard = g_Hooks.service(eService::DASHBOARD);
		for (const auto& instance : Game::im->GetInstances()) {
			if (!instance) continue;
			observed.worlds.push_back({ instance->View(), IsActivityZone(instance->GetMapID()) });
		}
		return observed;
	}

	class Actions final : public IActions {
	public:
		bool RunMigrations(std::string& error) override {
			LOG("Live update: running the new build's database migrations");
			try {
				MigrationRunner::RunMigrations();
				MigrationRunner::RunSQLiteMigrations();
				Database::Get()->Commit();
			} catch (const std::exception& ex) {
				error = std::string("The database migrations failed: ") + ex.what();
				LOG("Live update: %s", error.c_str());
				return false;
			}
			return true;
		}

		void RetireService(eService service) override {
			LOG("Live update: retiring the %s", ServiceName(service));
			g_Hooks.retire(service);
		}

		void StopService(eService service) override {
			LOG("Live update: stopping the %s", ServiceName(service));
			g_Hooks.stop(service);
		}

		void StartService(eService service) override {
			LOG("Live update: starting the %s", ServiceName(service));
			g_Hooks.start(service);
		}

		void ChatReady() override {
			LOG("Live update: the new chat server is up; telling the worlds");
			g_Hooks.chatReady();
		}

		std::optional<uint32_t> StartWorld(uint32_t zone) override {
			const auto& started = Game::im->StartNewInstance(static_cast<LWOMAPID>(zone), 0);
			if (!started) return std::nullopt;
			LOG("Live update: started instance %u of zone %u", started->GetInstanceID(), zone);
			return started->GetInstanceID();
		}

		void SetDraining(uint32_t zone, uint32_t instance, bool draining) override {
			const auto& found = Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(zone), static_cast<LWOINSTANCEID>(instance));
			if (!found) return;
			LOG("Live update: zone %u instance %u %s", zone, instance, draining ? "takes no new players" : "takes players again");
			found->SetIsDraining(draining);
		}

		std::optional<uint32_t> Migrate(const Move& move, std::string& refusal) override {
			InstanceMigrationRequest request;
			request.requestId = MIGRATION_ID_BASE | (++g_NextMigrationId & 0x3FFFFFFF);
			request.kind = move.kind;
			request.zoneId = move.zone;
			request.sourceInstance = move.instance;
			request.targetInstance = move.target;
			request.warnSeconds = move.warnSeconds;
			request.shutdownSource = true;
			request.requestedBy = "live update";
			MigrationCoordinator::Options options;
			options.liveUpdate = true;
			options.prepare = move.prepare;
			options.prepareWaitSeconds = move.prepareWaitSeconds;
			options.playerWaitSeconds = move.playerWaitSeconds;
			const auto result = MigrationCoordinator::Start(request, options);
			if (result != InstanceMigration::eRefusal::NONE) {
				refusal = InstanceMigration::Describe(result);
				return std::nullopt;
			}
			return request.requestId;
		}

		void StopWorld(uint32_t zone, uint32_t instance) override {
			const auto& found = Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(zone), static_cast<LWOINSTANCEID>(instance));
			if (found) found->Shutdown();
		}
	};

	Actions g_Actions;

	void LogChanges() {
		const auto& units = g_Machine.Units();
		for (size_t i = 0; i < units.size(); i++) {
			const auto& unit = units[i];
			auto& logged = g_Logged[i];
			if (logged.first == unit.state && logged.second == unit.message) continue;
			logged = { unit.state, unit.message };
			if (unit.kind == eUnitKind::WORLD) {
				LOG("Live update %u: zone %u clone %u instance %u%s: %s%s%s", g_UpdateId, unit.zone, unit.clone, unit.instance,
					unit.replacement ? (" -> " + std::to_string(unit.replacement)).c_str() : "", StateName(unit.state),
					unit.message.empty() ? "" : ": ", unit.message.c_str());
			} else {
				LOG("Live update %u: %s: %s%s%s", g_UpdateId, KindName(unit.kind), StateName(unit.state), unit.message.empty() ? "" : ": ", unit.message.c_str());
			}
		}
		if (g_LoggedPhase != g_Machine.Phase()) {
			g_LoggedPhase = g_Machine.Phase();
			LOG("Live update %u: %s%s%s", g_UpdateId, PhaseName(g_LoggedPhase), g_Machine.Message().empty() ? "" : ": ", g_Machine.Message().c_str());
		}
	}

	void Publish(bool phaseOrFinishChanged) {
		g_Machine.FillStatus(g_Status);
		g_Status.updateId = g_UpdateId;
		if (IsFinished(g_Status.phase) && g_Status.finishedAt == 0) g_Status.finishedAt = Now();
		LogChanges();
		if (g_Hooks.publish) g_Hooks.publish(g_Status, phaseOrFinishChanged);
	}

	size_t FinishedUnits() {
		return static_cast<size_t>(std::count_if(g_Machine.Units().begin(), g_Machine.Units().end(), [](const Unit& unit) { return IsFinished(unit.state); }));
	}

	// The binaries the update starts; a build still being written would fail to start
	bool CheckBinaries(std::string& error) {
		const auto dir = BinaryPathFinder::GetBinaryDir();
		std::vector<std::string> names{ "WorldServer", "AuthServer", "ChatServer" };
		if (Game::config->GetValue("enable_dashboard") == "1") names.push_back("DashboardServer");
		if (Game::config->GetValue("enable_ugc_server") == "1") names.push_back("UgcServer");
		for (auto name : names) {
#ifdef _WIN32
			name += ".exe";
#endif
			std::error_code ec;
			const auto path = dir / name;
			if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) == 0) {
				error = name + " is missing from " + dir.string() + " (is a build still running?)";
				return false;
			}
		}
		return true;
	}
}

void LiveUpdateCoordinator::Initialize(Hooks hooks) {
	g_Hooks = std::move(hooks);
}

bool LiveUpdateCoordinator::Start(const std::string& by, LWOOBJID requesterId, int32_t warnSeconds, std::string& error) {
	if (g_Machine.IsRunning()) {
		error = "A live update is already running";
		return false;
	}
	if (!CheckBinaries(error)) return false;
	const auto settings = ReadSettings(warnSeconds);
	const auto observed = Observe();
	if (!g_Machine.Start(settings, observed, Clock::now(), error)) return false;
	g_OldInstances.clear();
	for (const auto& world : observed.worlds) {
		if (!world.view.shuttingDown) g_OldInstances.insert({ world.view.zoneId, world.view.instanceId });
	}
	g_OldMarked = false;
	g_UpdateId++;
	g_Logged.clear();
	g_LoggedPhase = ePhase::IDLE;
	g_Status = LiveUpdateStatus{};
	g_Status.startedAt = Now();
	g_Status.by = by;
	g_Status.requesterId = requesterId;
	g_NextTick = {};
	LOG("Live update %u started by %s: %zu world instance(s), warn %u s, %u at a time", g_UpdateId, by.c_str(),
		g_Machine.Units().size() - 5, settings.warnSeconds, settings.parallelWorlds);
	Publish(true);
	return true;
}

void LiveUpdateCoordinator::HandleRequest(const LiveUpdateRequest& request) {
	switch (request.action) {
	case eAction::START: {
		std::string error;
		const auto by = request.requestedBy.empty() ? std::string("unknown") : request.requestedBy;
		if (!Start(by, request.requesterId, request.warnSeconds, error)) {
			LOG("Live update requested by %s refused: %s", by.c_str(), error.c_str());
			// The status tells whoever asked why (the running one's, or the last one's with the reason)
			auto status = Status();
			status.requesterId = request.requesterId;
			status.message = error;
			if (g_Hooks.publish) g_Hooks.publish(status, true);
		}
		return;
	}
	case eAction::CANCEL:
		if (g_Machine.Cancel()) {
			LOG("Live update %u cancelled by %s", g_UpdateId, request.requestedBy.c_str());
			Publish(true);
		}
		return;
	case eAction::STATUS: {
		auto status = Status();
		if (request.requesterId != LWOOBJID_EMPTY) status.requesterId = request.requesterId;
		if (g_Hooks.publish) g_Hooks.publish(status, request.requesterId != LWOOBJID_EMPTY);
		return;
	}
	}
}

void LiveUpdateCoordinator::OnMigrationStatus(const MigrationStatus& status) {
	if (!g_Machine.IsRunning()) return;
	g_Machine.OnMigration(status);
}

void LiveUpdateCoordinator::Update() {
	if (!g_Machine.IsRunning()) return;
	const auto now = Clock::now();
	if (now < g_NextTick) return;
	g_NextTick = now + TICK_INTERVAL;
	const auto phaseBefore = g_Machine.Phase();
	const auto finishedBefore = FinishedUnits();
	if (g_Machine.Tick(Observe(), now, g_Actions)) Publish(phaseBefore != g_Machine.Phase() || finishedBefore != FinishedUnits());
	MarkOldInstances();
}

namespace {
void MarkOldInstances() {
	if (g_OldMarked || g_Machine.Units().empty()) return;
	const auto database = g_Machine.Units().front().state;
	if (database != eUnitState::STOPPED && database != eUnitState::SKIPPED) return;
	g_OldMarked = true;
	// Nobody new goes to an instance on the old binaries: requests start new ones. Properties aren't moved; they stop
	// once everyone left (OutdatedInstances.h)
	const auto marked = Game::im->MarkOutdated([](const Instance& instance) {
		return g_OldInstances.contains({ instance.GetMapID(), instance.GetInstanceID() });
	});
	LOG("Live update %u: %u instance(s) on the old build take nobody new; properties among them stop once empty", g_UpdateId, marked);
}
}

void LiveUpdateCoordinator::Abort(const std::string& why) {
	if (!g_Machine.IsRunning()) return;
	g_Machine.Abort(why);
	Publish(true);
}

bool LiveUpdateCoordinator::IsRunning() {
	return g_Machine.IsRunning();
}

LiveUpdateStatus LiveUpdateCoordinator::Status() {
	auto status = g_Status;
	g_Machine.FillStatus(status);
	status.updateId = g_UpdateId;
	return status;
}
