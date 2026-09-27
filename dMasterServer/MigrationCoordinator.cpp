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
		LWOOBJID requester{};
		int reserved{};
		uint16_t moved{};
		uint16_t failed{};
		Clock::time_point deadline{};
		std::string by;
	};

	std::map<uint32_t, Migration> g_Active;
	std::function<void(const MigrationStatus&)> g_Reporter;

	const InstancePtr& FindInstance(uint32_t zone, uint32_t instance) {
		return Game::im->FindInstanceWithPrivate(static_cast<LWOMAPID>(zone), static_cast<LWOINSTANCEID>(instance));
	}

	InstanceView View(const Instance& instance) {
		InstanceView view;
		view.zoneId = instance.GetMapID();
		view.instanceId = instance.GetInstanceID();
		view.cloneId = instance.GetCloneID();
		view.players = instance.GetCurrentClientCount();
		view.softCap = instance.GetSoftCap();
		view.hardCap = instance.GetHardCap();
		view.reserved = instance.GetReserved();
		view.ready = instance.GetIsReady();
		view.isPrivate = instance.GetIsPrivate();
		view.shuttingDown = instance.GetIsShuttingDown() || instance.GetShutdownComplete();
		view.draining = instance.GetIsDraining();
		return view;
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
		if (!g_Reporter) return;
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
		g_Reporter(status);
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
		order.mythranShift = !migration.seamless;
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

eRefusal MigrationCoordinator::Start(const InstanceMigrationRequest& request) {
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

	const auto refuse = [&migration](eRefusal refusal) {
		Report(migration, eState::FAILED, Describe(refusal));
		return refusal;
	};

	if (g_Active.contains(migration.id)) return refuse(eRefusal::ALREADY_MIGRATING);
	// A raw pointer: starting an instance below grows the instance list, which moves the InstancePtrs
	Instance* source = FindInstance(request.zoneId, request.sourceInstance).get();
	if (!source) return refuse(eRefusal::NOT_RUNNING);
	auto sourceView = View(*source);
	if (const auto refusal = CheckSource(sourceView, IsActivityZone(request.zoneId)); refusal != eRefusal::NONE) return refuse(refusal);
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
	} else {
		// A fresh world server, started from the binary on disk now: this is how a live update takes over
		const auto& started = Game::im->StartNewInstance(source->GetMapID(), source->GetCloneID());
		if (!started) return refuse(eRefusal::MASTER_SHUTTING_DOWN);
		target = started.get();
		migration.startedTarget = true;
	}

	migration.target = target->GetInstanceID();
	// Seats for everyone there now; a few may still arrive while draining, the hard cap is the real limit
	migration.reserved = source->GetCurrentClientCount();
	target->SetReserved(target->GetReserved() + migration.reserved);
	source->SetIsDraining(true);

	LOG("Migration %u requested by %s: %s zone %u instance %u (%i player(s)) -> instance %u", migration.id, migration.by.c_str(),
		KindName(migration.kind), migration.zone, migration.source, source->GetCurrentClientCount(), migration.target);

	auto& active = g_Active[migration.id] = migration;
	if (target->GetIsReady()) {
		SendOrder(active, *source, *target);
	} else {
		active.state = eState::STARTING_TARGET;
		active.deadline = Clock::now() + TARGET_START_TIMEOUT;
		Report(active, eState::STARTING_TARGET, "Starting instance " + std::to_string(active.target),
			static_cast<uint16_t>(source->GetCurrentClientCount()));
	}
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
		if (migration.state == eState::STARTING_TARGET) {
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
