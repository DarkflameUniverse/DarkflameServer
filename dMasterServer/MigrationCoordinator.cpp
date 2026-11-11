#include "MigrationCoordinator.h"

#include <chrono>
#include <map>

#include "BitStreamUtils.h"
#include "CDActivitiesTable.h"
#include "CDClientManager.h"
#include "Game.h"
#include "InstanceManager.h"
#include "MasterPackets.h"
#include "Logger.h"
#include "MessageType/Master.h"
#include "ServiceType.h"
#include "dServer.h"

using namespace InstanceMigration;

namespace {
	using Clock = std::chrono::steady_clock;
	// A new world server takes a few seconds to load its zone; big zones on slow disks take longer
	constexpr auto TARGET_START_TIMEOUT = std::chrono::seconds(120);
	// After the warning, moving everyone must be over by then (the source world gives up on stuck players sooner)
	constexpr auto MOVE_TIMEOUT = std::chrono::seconds(180);
	constexpr uint16_t PLAYERS_PER_SECOND = 10;

	struct Migration {
		uint32_t id{};
		eKind kind{};
		eState state{};
		uint32_t zone{};
		uint32_t clone{};
		uint32_t source{};
		uint32_t target{};
		uint16_t warnSeconds{};
		bool shutdownSource{};
		bool startedTarget{}; // we started the target for this migration
		bool seamless{};
		bool liveUpdate{};
		bool prepare{};
		uint16_t prepareWaitSeconds{};
		uint16_t playerWaitSeconds{ MigratePlayersOrder::DEFAULT_MAX_WAIT_SECONDS };
		LWOOBJID requester{};
		int reserved{};
		uint16_t moved{};
		uint16_t failed{};
		Clock::time_point deadline{};
		std::string by;
	};

	std::map<uint32_t, Migration> g_Active;
	std::function<void(const MigrationStatus&)> g_Reporter;
	std::function<void(const MigrationStatus&)> g_Observer;

	const InstancePtr& FindInstance(uint32_t zone, uint32_t instance) {
		return Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(zone), static_cast<LWOINSTANCEID>(instance));
	}

	InstanceView View(const Instance& instance) {
		return instance.View();
	}

	// Races, minigames and other activities run in their own zones with state that only lives in that world
	bool IsActivityZone(uint32_t zone) {
		auto* activities = CDClientManager::GetTable<CDActivitiesTable>();
		if (!activities) return false;
		return !activities->Query([zone](const CDActivities& activity) { return activity.instanceMapID == zone; }).empty();
	}

	bool IsTakingPart(uint32_t zone, uint32_t instance) {
		for (const auto& [id, migration] : g_Active) {
			if (migration.zone == zone && (migration.source == instance || migration.target == instance)) return true;
		}
		return false;
	}

	void Report(const Migration& migration, eState state, const std::string& message, uint16_t remaining = 0) {
		LOG("Migration %u (%s zone %u instance %u -> %u): %s%s%s", migration.id, KindName(migration.kind), migration.zone, migration.source,
			migration.target, StateName(state), message.empty() ? "" : ": ", message.c_str());
		if (!g_Reporter && !g_Observer) return;
		MigrationStatus status;
		status.migrationId = migration.id;
		status.state = state;
		status.kind = migration.kind;
		status.zoneId = migration.zone;
		status.sourceInstance = migration.source;
		status.targetInstance = migration.target;
		status.moved = migration.moved;
		status.failed = migration.failed;
		status.requesterId = migration.requester;
		status.remaining = remaining;
		status.message = message;
		if (g_Observer) g_Observer(status);
		// In game only the GM who asked hears about it
		if (g_Reporter && status.requesterId != LWOOBJID_EMPTY) g_Reporter(status);
	}

	void ReleaseSeats(Migration& migration) {
		if (migration.reserved == 0) return;
		if (const auto& target = FindInstance(migration.zone, migration.target)) target->SetReserved(target->GetReserved() - migration.reserved);
		migration.reserved = 0;
	}

	// Tell the source world to stop sending players (a target port of 0 cancels)
	void SendCancel(const Migration& migration) {
		const auto& source = FindInstance(migration.zone, migration.source);
		if (!source) return;
		MigratePlayersOrder order;
		order.migrationId = migration.id;
		order.targetZone = migration.zone;
		order.targetInstance = migration.target;
		order.targetPort = 0;
		MasterPackets::SendTo(source->GetSysAddr(), order);
	}

	// Ends a migration. The source goes back to taking players unless it is being shut down.
	void Finish(std::map<uint32_t, Migration>::iterator it, bool success, const std::string& message) {
		auto& migration = it->second;
		ReleaseSeats(migration);
		const auto& source = FindInstance(migration.zone, migration.source);
		if (success && migration.shutdownSource && source) {
			// Draining stays set: nothing is sent to it while it shuts down
			source->Shutdown();
		} else if (source && migration.prepare && migration.moved > 0) {
			// A property some players already went to: the new instance owns it now, and the old one stays frozen
			// (it is shut down once empty); nobody new goes to the old one
			LOG("Migration %u: the property's old instance %u keeps draining (players already went to %u)", migration.id, migration.source, migration.target);
		} else if (source) {
			source->SetIsDraining(false);
		}
		// A fresh instance nobody went to is shut down again (it would idle for half an hour otherwise)
		if (!success && migration.startedTarget) {
			const auto& target = FindInstance(migration.zone, migration.target);
			if (target && target->GetCurrentClientCount() == 0) target->Shutdown();
		}
		Report(migration, success ? eState::DONE : eState::FAILED, message);
		g_Active.erase(it);
	}

	void SendOrder(Migration& migration, const Instance& source, const Instance& target) {
		MigratePlayersOrder order;
		order.migrationId = migration.id;
		order.targetZone = target.GetMapID();
		order.targetInstance = target.GetInstanceID();
		order.targetClone = target.GetCloneID();
		order.targetIp = target.GetIP();
		order.targetPort = static_cast<uint16_t>(target.GetPort());
		order.warnSeconds = migration.warnSeconds;
		order.playersPerSecond = PLAYERS_PER_SECOND;
		// Without a loading screen the "dimensional shift" notice would be the only sign; leave it out then
		order.seamless = migration.seamless;
		// Character selection has no maintenance notice to show
		order.mythranShift = !migration.seamless && source.GetMapID() != 0;
		order.maxWaitSeconds = migration.playerWaitSeconds;
		MasterPackets::SendTo(source.GetSysAddr(), order);

		migration.state = migration.warnSeconds > 0 ? eState::WARNING : eState::MOVING;
		migration.deadline = Clock::now() + std::chrono::seconds(migration.warnSeconds) + MOVE_TIMEOUT;
		Report(migration, migration.state, migration.warnSeconds > 0 ? "Players were warned" : "Moving players",
			static_cast<uint16_t>(source.GetCurrentClientCount()));
	}
}

void MigrationCoordinator::SetReporter(std::function<void(const MigrationStatus&)> reporter) {
	g_Reporter = std::move(reporter);
}

void MigrationCoordinator::SetObserver(std::function<void(const MigrationStatus&)> observer) {
	g_Observer = std::move(observer);
}

namespace {
	// The new instance (REPLACE): a fresh world server from the binary on disk now, so an update takes effect. A private
	// instance's replacement gets the same password; the source is draining already, so the password finds the new one.
	Instance* StartTarget(Instance& source) {
		if (source.GetIsPrivate()) {
			source.SetIsDraining(true);
			const auto& started = Game::im->CreatePrivateInstance(source.GetMapID(), source.GetCloneID(), source.GetPassword());
			return started ? started.get() : nullptr;
		}
		const auto& started = Game::im->StartNewInstance(source.GetMapID(), source.GetCloneID());
		return started ? started.get() : nullptr;
	}

	// Seats held, the source draining, and the order sent once the target is ready
	void Begin(Migration& migration, Instance& source, Instance& target) {
		migration.target = target.GetInstanceID();
		// Seats for everyone there now; a few may still arrive while draining, the hard cap is the real limit
		migration.reserved = source.GetCurrentClientCount();
		target.SetReserved(target.GetReserved() + migration.reserved);
		source.SetIsDraining(true);
		if (target.GetIsReady()) {
			SendOrder(migration, source, target);
		} else {
			migration.state = eState::STARTING_TARGET;
			migration.deadline = Clock::now() + TARGET_START_TIMEOUT;
			Report(migration, eState::STARTING_TARGET, "Starting instance " + std::to_string(migration.target),
				static_cast<uint16_t>(source.GetCurrentClientCount()));
		}
	}
}

eRefusal MigrationCoordinator::Start(const InstanceMigrationRequest& request, const Options& options) {
	Migration migration;
	migration.id = request.requestId;
	migration.kind = request.kind;
	migration.zone = request.zoneId;
	migration.source = request.sourceInstance;
	migration.warnSeconds = std::min(request.warnSeconds, InstanceMigrationRequest::MAX_WARN_SECONDS);
	migration.shutdownSource = request.shutdownSource;
	migration.seamless = request.seamless;
	migration.requester = request.requesterId;
	migration.by = request.requestedBy;
	migration.liveUpdate = options.liveUpdate;
	migration.prepare = options.prepare;
	migration.prepareWaitSeconds = std::min(options.prepareWaitSeconds, MigratePrepare::MAX_WAIT_SECONDS);
	migration.playerWaitSeconds = std::min(options.playerWaitSeconds, MigratePlayersOrder::MAX_MAX_WAIT_SECONDS);

	const auto refuse = [&migration](eRefusal refusal) {
		Report(migration, eState::FAILED, Describe(refusal));
		return refusal;
	};

	if (g_Active.contains(migration.id)) return refuse(eRefusal::ALREADY_MIGRATING);
	// A raw pointer: starting an instance below grows the instance list, which moves the InstancePtrs
	Instance* source = FindInstance(request.zoneId, request.sourceInstance).get();
	if (!source) return refuse(eRefusal::NOT_RUNNING);
	auto sourceView = View(*source);
	const auto sourceRefusal = options.liveUpdate ? CheckLiveUpdateSource(sourceView) : CheckSource(sourceView, IsActivityZone(request.zoneId));
	if (sourceRefusal != eRefusal::NONE) return refuse(sourceRefusal);
	if (IsTakingPart(request.zoneId, request.sourceInstance)) return refuse(eRefusal::ALREADY_MIGRATING);
	migration.clone = source->GetCloneID();

	Instance* target = nullptr;
	if (request.kind == eKind::MERGE) {
		std::vector<InstanceView> views;
		for (const auto& instance : Game::im->GetInstances()) {
			// Instances taking part in another migration don't count as targets
			if (instance && !IsTakingPart(instance->GetMapID(), instance->GetInstanceID())) views.push_back(View(*instance));
		}
		uint32_t targetId = request.targetInstance;
		if (targetId == 0) {
			const auto picked = PickMergeTarget(views, sourceView);
			if (!picked) return refuse(eRefusal::NO_TARGET);
			targetId = *picked;
		}
		const auto& found = FindInstance(request.zoneId, targetId);
		if (!found) return refuse(eRefusal::NOT_RUNNING);
		if (IsTakingPart(request.zoneId, targetId)) return refuse(eRefusal::ALREADY_MIGRATING);
		if (const auto refusal = CheckMergeTarget(View(*found), sourceView); refusal != eRefusal::NONE) return refuse(refusal);
		target = found.get();
	} else if (migration.prepare) {
		// A property: the new instance loads it from the database when it starts, so the source saves and freezes it
		// first (HandleStatus starts the target once it is PREPARED). Visitors still go to the source meanwhile.
		LOG("Migration %u requested by %s: preparing zone %u clone %u instance %u (%i player(s))", migration.id, migration.by.c_str(),
			migration.zone, migration.clone, migration.source, source->GetCurrentClientCount());
		auto& active = g_Active[migration.id] = migration;
		active.state = eState::PREPARING;
		active.deadline = Clock::now() + std::chrono::seconds(active.prepareWaitSeconds) + TARGET_START_TIMEOUT;
		MigratePrepare prepare;
		prepare.migrationId = active.id;
		prepare.maxWaitSeconds = active.prepareWaitSeconds;
		MasterPackets::SendTo(source->GetSysAddr(), prepare);
		Report(active, eState::PREPARING, "Saving the property", static_cast<uint16_t>(source->GetCurrentClientCount()));
		return eRefusal::NONE;
	} else {
		// A fresh world server, started from the binary on disk now: this is how a live update takes over
		target = StartTarget(*source);
		if (!target) {
			source->SetIsDraining(false);
			return refuse(eRefusal::MASTER_SHUTTING_DOWN);
		}
		migration.startedTarget = true;
		// Starting it grew the instance list, which moves the InstancePtrs but not the Instances: source is still good
	}

	LOG("Migration %u requested by %s: %s zone %u instance %u (%i player(s)) -> instance %u", migration.id, migration.by.c_str(),
		KindName(migration.kind), migration.zone, migration.source, source->GetCurrentClientCount(), target->GetInstanceID());

	auto& active = g_Active[migration.id] = migration;
	Begin(active, *source, *target);
	return eRefusal::NONE;
}

void MigrationCoordinator::HandleStatus(const SystemAddress& from, const MigrationStatus& status) {
	const auto it = g_Active.find(status.migrationId);
	if (it == g_Active.end()) return;
	auto& migration = it->second;
	const auto& source = FindInstance(migration.zone, migration.source);
	if (!source || source->GetSysAddr() != from) return; // only the source world runs it

	migration.moved = status.moved;
	migration.failed = status.failed;
	if (status.state == eState::DONE) return Finish(it, true, status.message);
	if (status.state == eState::FAILED) return Finish(it, false, status.message);
	if (status.state == eState::PREPARED) {
		if (migration.state != eState::PREPARING) return;
		// Saved and frozen: now the new instance may load the property
		Instance* sourceInstance = source.get();
		Instance* target = StartTarget(*sourceInstance);
		if (!target) {
			SendCancel(migration);
			return Finish(it, false, "Master refused to start the new instance");
		}
		migration.startedTarget = true;
		LOG("Migration %u: property of zone %u clone %u saved; instance %u -> %u", migration.id, migration.zone, migration.clone, migration.source, target->GetInstanceID());
		Report(migration, eState::PREPARED, status.message, static_cast<uint16_t>(sourceInstance->GetCurrentClientCount()));
		return Begin(migration, *sourceInstance, *target);
	}
	migration.state = status.state;
	Report(migration, status.state, status.message, status.remaining);
}

void MigrationCoordinator::HandleCarriedState(const SystemAddress& from, const CarriedPlayerState& state) {
	for (const auto& [id, migration] : g_Active) {
		if (migration.zone != state.targetZone || migration.target != state.targetInstance) continue;
		const auto& source = FindInstance(migration.zone, migration.source);
		if (!source || source->GetSysAddr() != from) continue;
		const auto& target = FindInstance(migration.zone, migration.target);
		if (!target) return;
		MasterPackets::SendTo(target->GetSysAddr(), state);
		return;
	}
}

void MigrationCoordinator::OnInstanceGone(const Instance& instance) {
	for (auto it = g_Active.begin(); it != g_Active.end();) {
		const auto& migration = it->second;
		if (migration.zone != instance.GetMapID() || (migration.source != instance.GetInstanceID() && migration.target != instance.GetInstanceID())) {
			++it;
			continue;
		}
		const bool wasSource = migration.source == instance.GetInstanceID();
		// Its players were saved when it stopped; with a shut down planned that is a finished drain
		if (wasSource && migration.state == eState::MOVING && migration.shutdownSource) {
			auto done = it++;
			Finish(done, true, "The old instance stopped");
			continue;
		}
		if (!wasSource) SendCancel(migration);
		auto failed = it++;
		Finish(failed, false, wasSource ? "The instance being emptied stopped" : "The target instance stopped");
	}
}

void MigrationCoordinator::Update() {
	const auto now = Clock::now();
	for (auto it = g_Active.begin(); it != g_Active.end();) {
		auto& migration = it->second;
		auto current = it++;
		if (migration.state == eState::PREPARING) {
			if (!FindInstance(migration.zone, migration.source)) {
				Finish(current, false, "The instance being emptied stopped");
			} else if (now > migration.deadline) {
				SendCancel(migration);
				Finish(current, false, "The property wasn't saved in time");
			}
		} else if (migration.state == eState::STARTING_TARGET) {
			const auto& source = FindInstance(migration.zone, migration.source);
			const auto& target = FindInstance(migration.zone, migration.target);
			if (!source || !target) {
				Finish(current, false, !source ? "The instance being emptied stopped" : "The new instance stopped while starting");
			} else if (target->GetIsReady()) {
				SendOrder(migration, *source, *target);
			} else if (now > migration.deadline) {
				Finish(current, false, "The new instance didn't start in time");
			}
		} else if (now > migration.deadline) {
			SendCancel(migration);
			Finish(current, false, "Timed out moving players");
		}
	}
}
