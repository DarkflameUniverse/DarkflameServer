#include <gtest/gtest.h>

#include <algorithm>
#include <map>

#include "LiveUpdateMachine.h"
#include "master/InstanceMigration.h"
#include "master/LiveUpdate.h"

using namespace LiveUpdate;
using InstanceMigration::eKind;
using InstanceMigration::InstanceView;

namespace {
	InstanceView World(uint32_t zone, uint32_t instance, int32_t players, uint32_t clone = 0) {
		InstanceView view;
		view.zoneId = zone;
		view.instanceId = instance;
		view.cloneId = clone;
		view.players = players;
		view.softCap = 8;
		view.hardCap = 12;
		view.ready = true;
		return view;
	}

	// Plays master: records what the machine asks for, and lets the test change what "master" sees
	struct FakeMaster final : public IActions {
		Observed observed;
		bool migrationsFail = false;
		bool refuseMoves = false;
		uint32_t nextInstance = 100;
		uint32_t nextMigration = 1;
		std::vector<std::string> log;
		std::vector<Move> moves;
		std::map<std::pair<uint32_t, uint32_t>, bool> draining;
		int chatReady = 0;

		FakeMaster() {
			observed.ugc = { true, true, 1 };
			observed.auth = { true, true, 1 };
			observed.chat = { true, true, 1 };
			observed.dashboard = { true, true, 1 };
		}

		ServiceView& Service(eService service) {
			switch (service) {
			case eService::UGC: return observed.ugc;
			case eService::AUTH: return observed.auth;
			case eService::CHAT: return observed.chat;
			case eService::DASHBOARD: return observed.dashboard;
			}
			return observed.auth;
		}

		void Add(const InstanceView& view, bool activity = false) { observed.worlds.push_back({ view, activity }); }

		WorldView* Find(uint32_t zone, uint32_t instance) {
			for (auto& world : observed.worlds) {
				if (world.view.zoneId == zone && world.view.instanceId == instance) return &world;
			}
			return nullptr;
		}

		void Remove(uint32_t zone, uint32_t instance) {
			std::erase_if(observed.worlds, [&](const WorldView& w) { return w.view.zoneId == zone && w.view.instanceId == instance; });
		}

		// The old server went away and master started the new one, which connected
		void Replace(eService service) {
			auto& view = Service(service);
			view.online = true;
			view.connects++;
		}

		bool RunMigrations(std::string& error) override {
			log.push_back("migrations");
			if (migrationsFail) error = "boom";
			return !migrationsFail;
		}
		void RetireService(eService service) override { log.push_back(std::string("retire ") + ServiceName(service)); }
		void StopService(eService service) override { log.push_back(std::string("stop ") + ServiceName(service)); }
		void StartService(eService service) override { log.push_back(std::string("start ") + ServiceName(service)); }
		void ChatReady() override { chatReady++; }
		std::optional<uint32_t> StartWorld(uint32_t zone) override {
			const auto id = nextInstance++;
			auto view = World(zone, id, 0);
			view.ready = false;
			Add(view);
			log.push_back("start world " + std::to_string(zone) + "/" + std::to_string(id));
			return id;
		}
		void SetDraining(uint32_t zone, uint32_t instance, bool value) override {
			draining[{ zone, instance }] = value;
			if (auto* world = Find(zone, instance)) world->view.draining = value;
		}
		std::optional<uint32_t> Migrate(const Move& move, std::string& refusal) override {
			if (refuseMoves) {
				refusal = "refused";
				return std::nullopt;
			}
			moves.push_back(move);
			return nextMigration++;
		}
		void StopWorld(uint32_t zone, uint32_t instance) override {
			log.push_back("stop world " + std::to_string(zone) + "/" + std::to_string(instance));
		}

		bool Draining(uint32_t zone, uint32_t instance) const {
			const auto it = draining.find({ zone, instance });
			return it != draining.end() && it->second;
		}

		bool Logged(const std::string& entry) const { return std::find(log.begin(), log.end(), entry) != log.end(); }
	};

	MigrationStatus Status(uint32_t id, InstanceMigration::eState state, uint32_t target = 0, uint16_t moved = 0) {
		MigrationStatus status;
		status.migrationId = id;
		status.state = state;
		status.targetInstance = target;
		status.moved = moved;
		return status;
	}

	struct Harness {
		FakeMaster master;
		Machine machine;
		Settings settings;
		Clock::time_point now = Clock::time_point{} + std::chrono::hours(1);

		Harness() {
			settings.parallelWorlds = 4;
			settings.keepZones = { 0, 1000 };
		}

		void Start() {
			std::string error;
			ASSERT_TRUE(machine.Start(settings, master.observed, now, error)) << error;
		}

		void Tick(std::chrono::seconds advance = std::chrono::seconds(0)) {
			now += advance;
			machine.Tick(master.observed, now, master);
		}

		const LiveUpdate::Unit& Row(eUnitKind kind) const {
			for (const auto& unit : machine.Units()) if (unit.kind == kind) return unit;
			return machine.Units().front();
		}

		const LiveUpdate::Unit& WorldUnit(uint32_t zone, uint32_t instance) const {
			for (const auto& unit : machine.Units()) if (unit.kind == eUnitKind::WORLD && unit.zone == zone && unit.instance == instance) return unit;
			return machine.Units().front();
		}

		// Auth, chat and UGC come back; chat settles
		void ServicesComeBack() {
			Tick();
			master.Replace(eService::UGC);
			master.Replace(eService::AUTH);
			master.Replace(eService::CHAT);
			Tick();
			Tick(settings.chatSettle);
		}
	};
}

TEST(LiveUpdateTest, PlanForEachKindOfInstance) {
	EXPECT_EQ(PlanWorld(World(1100, 1, 0), false, false, false), eWorldPlan::STOP);
	EXPECT_EQ(PlanWorld(World(1000, 1, 0), false, true, false), eWorldPlan::REPLACE_EMPTY);
	EXPECT_EQ(PlanWorld(World(1000, 2, 0), false, true, true), eWorldPlan::STOP); // the zone has its new one already
	EXPECT_EQ(PlanWorld(World(0, 1, 0), false, false, false), eWorldPlan::REPLACE_EMPTY);
	EXPECT_EQ(PlanWorld(World(1100, 1, 3), false, false, false), eWorldPlan::MOVE);
	EXPECT_EQ(PlanWorld(World(1150, 1, 2, 77), false, false, false), eWorldPlan::MOVE);
	EXPECT_EQ(PlanWorld(World(1203, 1, 2), true, false, false), eWorldPlan::WAIT_THEN_MOVE);
	EXPECT_EQ(PlanWorld(World(0, 1, 5), false, true, false), eWorldPlan::WAIT_THEN_MOVE);
	// An empty property or private instance is never kept going
	EXPECT_EQ(PlanWorld(World(1000, 1, 0, 5), false, true, false), eWorldPlan::STOP);
	auto privateWorld = World(1000, 1, 0);
	privateWorld.isPrivate = true;
	EXPECT_EQ(PlanWorld(privateWorld, false, true, false), eWorldPlan::STOP);
}

TEST(LiveUpdateTest, RoutingSkipsDrainingAndFullInstances) {
	using InstanceMigration::AcceptsNewPlayers;
	auto world = World(1100, 1, 3);
	EXPECT_TRUE(AcceptsNewPlayers(world, 1100, 0, false));
	EXPECT_FALSE(AcceptsNewPlayers(world, 1200, 0, false));
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 5, false));
	world.draining = true;
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, false));
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, true));
	world.draining = false;
	world.players = 8;
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, false)); // soft cap
	EXPECT_TRUE(AcceptsNewPlayers(world, 1100, 0, true));   // a friend may go up to the hard cap
	world.players = 6;
	world.reserved = 2; // seats held for players being moved in
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, false));
	world.reserved = 0;
	world.shuttingDown = true;
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, false));
	world.shuttingDown = false;
	world.isPrivate = true;
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, true));
	world.isPrivate = false;
	// On the old binary or old files: nobody new, friends and properties included
	world.outdated = true;
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, false));
	EXPECT_FALSE(AcceptsNewPlayers(world, 1100, 0, true));
	auto property = World(1150, 3, 1, 42);
	EXPECT_TRUE(AcceptsNewPlayers(property, 1150, 42, false));
	property.outdated = true;
	EXPECT_FALSE(AcceptsNewPlayers(property, 1150, 42, false));
	EXPECT_FALSE(AcceptsNewPlayers(property, 1150, 42, true));
	// A replacement still starting takes players: they wait for it
	auto starting = World(1100, 2, 0);
	starting.ready = false;
	EXPECT_TRUE(AcceptsNewPlayers(starting, 1100, 0, false));
}

TEST(LiveUpdateTest, LiveUpdateSourceCheck) {
	using InstanceMigration::CheckLiveUpdateSource;
	using InstanceMigration::eRefusal;
	EXPECT_EQ(CheckLiveUpdateSource(World(0, 1, 3)), eRefusal::NONE);
	EXPECT_EQ(CheckLiveUpdateSource(World(1150, 1, 3, 9)), eRefusal::NONE);
	auto draining = World(1203, 1, 1);
	draining.draining = true;
	EXPECT_EQ(CheckLiveUpdateSource(draining), eRefusal::NONE);
	auto starting = World(1100, 1, 0);
	starting.ready = false;
	EXPECT_EQ(CheckLiveUpdateSource(starting), eRefusal::NOT_READY);
	auto stopping = World(1100, 1, 0);
	stopping.shuttingDown = true;
	EXPECT_EQ(CheckLiveUpdateSource(stopping), eRefusal::SHUTTING_DOWN);
}

TEST(LiveUpdateTest, OnlyOneAtATime) {
	Harness h;
	h.Start();
	std::string error;
	EXPECT_FALSE(h.machine.Start(h.settings, h.master.observed, h.now, error));
	EXPECT_FALSE(error.empty());
}

TEST(LiveUpdateTest, ServersFirstThenWorldsThenDashboard) {
	Harness h;
	h.master.Add(World(1100, 1, 3));
	h.Start();
	h.Tick();
	EXPECT_TRUE(h.master.Logged("migrations"));
	EXPECT_TRUE(h.master.Logged("retire UGC server"));
	EXPECT_TRUE(h.master.Logged("retire auth server"));
	EXPECT_TRUE(h.master.Logged("retire chat server"));
	EXPECT_FALSE(h.master.Logged("retire dashboard"));
	EXPECT_EQ(h.Row(eUnitKind::UGC).state, eUnitState::DRAINING);
	EXPECT_EQ(h.Row(eUnitKind::CHAT).state, eUnitState::STOPPING);
	// No world moves before chat is back
	EXPECT_TRUE(h.master.moves.empty());

	// The old ones disconnect; master starts new ones
	h.master.observed.auth.online = false;
	h.master.observed.chat.online = false;
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::AUTH).state, eUnitState::STARTING);
	EXPECT_EQ(h.Row(eUnitKind::CHAT).state, eUnitState::STARTING);
	h.master.Replace(eService::AUTH);
	h.master.Replace(eService::CHAT);
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::AUTH).state, eUnitState::STOPPED);
	EXPECT_EQ(h.Row(eUnitKind::CHAT).state, eUnitState::READY);
	EXPECT_EQ(h.master.chatReady, 1);
	EXPECT_TRUE(h.master.moves.empty()) << "worlds wait for the worlds to reconnect to chat";
	h.Tick(h.settings.chatSettle);
	EXPECT_EQ(h.Row(eUnitKind::CHAT).state, eUnitState::STOPPED);
	h.Tick();
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(h.master.moves[0].kind, eKind::REPLACE);

	// The world is done; the dashboard waits for the UGC server too
	h.machine.OnMigration(Status(1, InstanceMigration::eState::DONE, 100, 3));
	h.master.Remove(1100, 1);
	h.Tick();
	EXPECT_EQ(h.WorldUnit(1100, 1).state, eUnitState::STOPPED);
	EXPECT_FALSE(h.master.Logged("retire dashboard"));
	h.master.Replace(eService::UGC);
	h.Tick();
	h.Tick();
	EXPECT_TRUE(h.master.Logged("retire dashboard"));
	EXPECT_EQ(h.machine.Phase(), ePhase::RUNNING);
	h.master.Replace(eService::DASHBOARD);
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::DASHBOARD).state, eUnitState::STOPPED);
	EXPECT_EQ(h.machine.Phase(), ePhase::DONE);
}

TEST(LiveUpdateTest, WorldGoesThroughEveryState) {
	Harness h;
	h.master.Add(World(1100, 1, 3));
	h.Start();
	h.ServicesComeBack();
	const auto& unit = h.WorldUnit(1100, 1);
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(unit.state, eUnitState::STARTING);
	EXPECT_FALSE(h.master.moves[0].prepare);
	EXPECT_EQ(h.master.moves[0].warnSeconds, h.settings.warnSeconds);
	EXPECT_EQ(h.master.moves[0].playerWaitSeconds, h.settings.playerWaitSeconds);

	h.machine.OnMigration(Status(1, InstanceMigration::eState::STARTING_TARGET, 100));
	EXPECT_EQ(unit.state, eUnitState::STARTING);
	EXPECT_EQ(unit.replacement, 100u);
	h.machine.OnMigration(Status(1, InstanceMigration::eState::WARNING, 100));
	EXPECT_EQ(unit.state, eUnitState::READY);
	h.machine.OnMigration(Status(1, InstanceMigration::eState::MOVING, 100, 1));
	EXPECT_EQ(unit.state, eUnitState::DRAINING);
	EXPECT_EQ(unit.moved, 1u);
	h.machine.OnMigration(Status(1, InstanceMigration::eState::DONE, 100, 3));
	EXPECT_EQ(unit.state, eUnitState::STOPPING);
	h.Tick();
	EXPECT_EQ(unit.state, eUnitState::STOPPING) << "the old one is still shutting down";
	h.master.Remove(1100, 1);
	h.Tick();
	EXPECT_EQ(unit.state, eUnitState::STOPPED);
	EXPECT_EQ(unit.moved, 3u);
}

TEST(LiveUpdateTest, OldInstanceThatNeverStopsFails) {
	Harness h;
	h.master.Add(World(1100, 1, 3));
	h.Start();
	h.ServicesComeBack();
	h.machine.OnMigration(Status(1, InstanceMigration::eState::DONE, 100, 3));
	h.Tick();
	h.Tick(h.settings.worldStartTimeout);
	EXPECT_EQ(h.WorldUnit(1100, 1).state, eUnitState::FAILED);
}

TEST(LiveUpdateTest, PropertiesAreNeverInThePlan) {
	EXPECT_FALSE(InReplacePlan(World(1150, 1, 2, 42)));
	EXPECT_FALSE(InReplacePlan(World(1150, 2, 0, 43)));
	EXPECT_TRUE(InReplacePlan(World(1100, 3, 2)));
	auto stopping = World(1100, 4, 0);
	stopping.shuttingDown = true;
	EXPECT_FALSE(InReplacePlan(stopping));

	Harness h;
	h.master.Add(World(1150, 1, 2, 42)); // busy property
	h.master.Add(World(1150, 2, 0, 43)); // empty property
	h.master.Add(World(1100, 3, 2));
	h.Start();
	size_t worlds = 0;
	for (const auto& unit : h.machine.Units()) {
		if (unit.kind != eUnitKind::WORLD) continue;
		worlds++;
		EXPECT_EQ(unit.clone, 0u);
	}
	EXPECT_EQ(worlds, 1u);
	h.ServicesComeBack();
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(h.master.moves[0].zone, 1100u);
	EXPECT_FALSE(h.master.moves[0].prepare);
	EXPECT_TRUE(h.master.log.end() == std::find_if(h.master.log.begin(), h.master.log.end(), [](const std::string& line) {
		return line.find("1150") != std::string::npos;
	})) << "nothing is done to a property";
}

TEST(LiveUpdateTest, ZoneWithANewInstanceAlreadyIsNotStartedAgain) {
	Harness h;
	h.master.Add(World(1000, 1, 0));
	h.Start();
	// Someone went to the zone after the update began: master started a new instance for them
	h.master.Add(World(1000, 50, 1));
	h.ServicesComeBack();
	h.Tick();
	EXPECT_EQ(h.WorldUnit(1000, 1).plan, eWorldPlan::STOP);
	EXPECT_TRUE(h.master.Logged("stop world 1000/1"));
	EXPECT_TRUE(std::none_of(h.master.log.begin(), h.master.log.end(), [](const std::string& line) { return line.rfind("start world 1000/", 0) == 0; }));
}

TEST(LiveUpdateTest, EmptyInstancesStopOrAreReplaced) {
	Harness h;
	h.master.Add(World(1100, 1, 0)); // nobody there, not kept: stopped
	h.master.Add(World(1000, 2, 0)); // Venture Explorer is kept: replaced
	h.master.Add(World(1000, 3, 0)); // a second empty one: just stopped
	h.Start();
	h.ServicesComeBack();
	EXPECT_TRUE(h.master.Logged("stop world 1100/1"));
	EXPECT_TRUE(h.master.Logged("start world 1000/100"));
	EXPECT_TRUE(h.master.Logged("stop world 1000/3"));
	EXPECT_FALSE(h.master.Logged("stop world 1000/2")) << "stopped only once the new one is ready";
	EXPECT_TRUE(h.master.Draining(1100, 1));
	EXPECT_EQ(h.WorldUnit(1000, 2).state, eUnitState::STARTING);

	h.master.Find(1000, 100)->view.ready = true;
	h.Tick();
	EXPECT_TRUE(h.master.Logged("stop world 1000/2"));
	EXPECT_EQ(h.WorldUnit(1000, 2).state, eUnitState::STOPPING);
	h.master.Remove(1100, 1);
	h.master.Remove(1000, 2);
	h.master.Remove(1000, 3);
	h.Tick();
	EXPECT_EQ(h.WorldUnit(1100, 1).state, eUnitState::STOPPED);
	EXPECT_EQ(h.WorldUnit(1000, 2).state, eUnitState::STOPPED);
	EXPECT_EQ(h.WorldUnit(1000, 3).state, eUnitState::STOPPED);
	EXPECT_TRUE(h.master.moves.empty());
}

TEST(LiveUpdateTest, SomeoneArrivingBeforeTheReplacementIsReadyIsMoved) {
	Harness h;
	h.master.Add(World(1000, 2, 0));
	h.Start();
	h.ServicesComeBack();
	h.master.Find(1000, 2)->view.players = 1;
	h.master.Find(1000, 100)->view.ready = true;
	h.Tick();
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(h.master.moves[0].kind, eKind::MERGE);
	EXPECT_EQ(h.master.moves[0].target, 100u);
	EXPECT_TRUE(h.master.Draining(1000, 2));
}

TEST(LiveUpdateTest, ReplacementThatNeverStartsFails) {
	Harness h;
	h.master.Add(World(1000, 2, 0));
	h.Start();
	h.ServicesComeBack();
	h.Tick(h.settings.worldStartTimeout);
	EXPECT_EQ(h.WorldUnit(1000, 2).state, eUnitState::FAILED);
	EXPECT_FALSE(h.master.Logged("stop world 1000/2")) << "the old one keeps running";
}

TEST(LiveUpdateTest, CharacterSelectDrainsThenMovesWhoIsLeft) {
	Harness h;
	h.master.Add(World(0, 1, 3));
	h.Start();
	h.ServicesComeBack();
	const auto& unit = h.WorldUnit(0, 1);
	EXPECT_EQ(unit.state, eUnitState::WAITING);
	EXPECT_TRUE(h.master.Logged("start world 0/100")) << "logins go to a new one at once";
	EXPECT_TRUE(h.master.Draining(0, 1));
	EXPECT_TRUE(h.master.moves.empty());
	EXPECT_EQ(h.machine.Busy(), 0u) << "waiting takes no slot";

	h.master.Find(0, 100)->view.ready = true;
	h.Tick(h.settings.charSelectWait);
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(h.master.moves[0].kind, eKind::MERGE);
	EXPECT_EQ(h.master.moves[0].target, 100u);
	EXPECT_EQ(unit.state, eUnitState::STARTING);
}

TEST(LiveUpdateTest, CharacterSelectThatEmptiesStops) {
	Harness h;
	h.master.Add(World(0, 1, 3));
	h.Start();
	h.ServicesComeBack();
	h.master.Find(0, 1)->view.players = 0;
	h.Tick();
	EXPECT_TRUE(h.master.Logged("stop world 0/1"));
	EXPECT_EQ(h.WorldUnit(0, 1).state, eUnitState::STOPPING);
	EXPECT_TRUE(h.master.moves.empty());
}

TEST(LiveUpdateTest, ActivitiesFinishFirst) {
	Harness h;
	h.master.Add(World(1203, 1, 2), true);
	h.Start();
	h.ServicesComeBack();
	EXPECT_EQ(h.WorldUnit(1203, 1).state, eUnitState::WAITING);
	EXPECT_TRUE(h.master.Draining(1203, 1));
	h.Tick(h.settings.activityWait - std::chrono::seconds(1));
	EXPECT_TRUE(h.master.moves.empty());
	h.Tick(std::chrono::seconds(1));
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_EQ(h.master.moves[0].kind, eKind::REPLACE);
}

TEST(LiveUpdateTest, AFewWorldsAtATime) {
	Harness h;
	h.settings.parallelWorlds = 2;
	for (uint32_t i = 1; i <= 5; i++) h.master.Add(World(1100, i, 2));
	h.Start();
	h.ServicesComeBack();
	EXPECT_EQ(h.master.moves.size(), 2u);
	EXPECT_EQ(h.machine.Busy(), 2u);
	h.Tick();
	EXPECT_EQ(h.master.moves.size(), 2u);
	h.machine.OnMigration(Status(1, InstanceMigration::eState::DONE, 100, 2));
	h.Tick();
	EXPECT_EQ(h.master.moves.size(), 3u) << "a stopping instance frees its slot";
}

TEST(LiveUpdateTest, BusiestFirstAndCharacterSelectBeforeAll) {
	Harness h;
	h.settings.parallelWorlds = 1;
	h.master.Add(World(1100, 1, 1));
	h.master.Add(World(1200, 2, 6));
	h.master.Add(World(0, 3, 2));
	h.Start();
	const auto& units = h.machine.Units();
	std::vector<uint32_t> order;
	for (const auto& unit : units) if (unit.kind == eUnitKind::WORLD) order.push_back(unit.zone);
	EXPECT_EQ(order, (std::vector<uint32_t>{ 0, 1200, 1100 }));
}

TEST(LiveUpdateTest, RefusedMoveFailsAndTakesPlayersAgain) {
	Harness h;
	h.master.refuseMoves = true;
	h.master.Add(World(1203, 1, 2), true);
	h.Start();
	h.ServicesComeBack();
	h.Tick(h.settings.activityWait);
	EXPECT_EQ(h.WorldUnit(1203, 1).state, eUnitState::FAILED);
	EXPECT_FALSE(h.master.Draining(1203, 1));
}

TEST(LiveUpdateTest, FailedMigrationsStopEverything) {
	Harness h;
	h.master.migrationsFail = true;
	h.master.Add(World(1100, 1, 3));
	h.Start();
	h.Tick();
	EXPECT_EQ(h.machine.Phase(), ePhase::FAILED);
	EXPECT_EQ(h.Row(eUnitKind::DATABASE).state, eUnitState::FAILED);
	EXPECT_FALSE(h.master.Logged("retire auth server"));
	EXPECT_TRUE(h.master.moves.empty());
	for (const auto& unit : h.machine.Units()) EXPECT_TRUE(IsFinished(unit.state));
	// And another can be started after
	std::string error;
	h.master.migrationsFail = false;
	EXPECT_TRUE(h.machine.Start(h.settings, h.master.observed, h.now, error));
}

TEST(LiveUpdateTest, MigrationsCanBeSwitchedOff) {
	Harness h;
	h.settings.runMigrations = false;
	h.Start();
	h.Tick();
	EXPECT_FALSE(h.master.Logged("migrations"));
	EXPECT_EQ(h.Row(eUnitKind::DATABASE).state, eUnitState::SKIPPED);
}

TEST(LiveUpdateTest, ServerThatDoesntComeBackIsStartedAgainThenFails) {
	Harness h;
	h.Start();
	h.Tick();
	h.master.observed.auth.online = false;
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::AUTH).state, eUnitState::STARTING);
	for (uint32_t i = 1; i <= h.settings.maxServiceStarts; i++) {
		h.Tick(h.settings.serviceTimeout);
		EXPECT_EQ(std::count(h.master.log.begin(), h.master.log.end(), "start auth server"), static_cast<long>(i));
	}
	h.Tick(h.settings.serviceTimeout);
	EXPECT_EQ(h.Row(eUnitKind::AUTH).state, eUnitState::FAILED);
}

TEST(LiveUpdateTest, ServerThatDoesntStopFails) {
	Harness h;
	h.Start();
	h.Tick();
	h.Tick(h.settings.serviceTimeout);
	EXPECT_EQ(h.Row(eUnitKind::AUTH).state, eUnitState::FAILED);
}

TEST(LiveUpdateTest, SlowUgcServerIsStopped) {
	Harness h;
	h.Start();
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::UGC).state, eUnitState::DRAINING);
	h.Tick(h.settings.ugcDrainTimeout - std::chrono::seconds(1));
	EXPECT_FALSE(h.master.Logged("stop UGC server"));
	h.Tick(std::chrono::seconds(1));
	EXPECT_TRUE(h.master.Logged("stop UGC server"));
	EXPECT_EQ(h.Row(eUnitKind::UGC).state, eUnitState::STOPPING);
	h.master.observed.ugc.online = false;
	h.Tick();
	h.master.Replace(eService::UGC);
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::UGC).state, eUnitState::STOPPED);
}

TEST(LiveUpdateTest, ServersNotRunningAreSkipped) {
	Harness h;
	h.master.observed.ugc.enabled = false;
	h.master.observed.chat.online = false;
	h.Start();
	h.Tick();
	EXPECT_EQ(h.Row(eUnitKind::UGC).state, eUnitState::SKIPPED);
	EXPECT_EQ(h.Row(eUnitKind::CHAT).state, eUnitState::SKIPPED);
	EXPECT_FALSE(h.master.Logged("retire chat server"));
}

TEST(LiveUpdateTest, CancelFinishesWhatIsUnderWay) {
	Harness h;
	h.settings.parallelWorlds = 1;
	h.master.Add(World(1100, 1, 2));
	h.master.Add(World(1200, 2, 1));
	h.Start();
	h.ServicesComeBack();
	ASSERT_EQ(h.master.moves.size(), 1u);
	EXPECT_TRUE(h.machine.Cancel());
	EXPECT_EQ(h.machine.Phase(), ePhase::CANCELLING);
	EXPECT_EQ(h.WorldUnit(1200, 2).state, eUnitState::SKIPPED);
	EXPECT_EQ(h.Row(eUnitKind::DASHBOARD).state, eUnitState::SKIPPED);
	h.machine.OnMigration(Status(1, InstanceMigration::eState::DONE, 100, 2));
	h.master.Remove(1100, 1);
	h.master.Replace(eService::UGC);
	h.Tick();
	h.Tick();
	EXPECT_EQ(h.master.moves.size(), 1u);
	EXPECT_FALSE(h.master.Logged("retire dashboard"));
	EXPECT_EQ(h.machine.Phase(), ePhase::CANCELLED);
	EXPECT_FALSE(h.machine.Cancel());
}

TEST(LiveUpdateTest, AbortEndsIt) {
	Harness h;
	h.master.Add(World(1100, 1, 2));
	h.Start();
	h.Tick();
	h.machine.Abort("Master is shutting down");
	EXPECT_EQ(h.machine.Phase(), ePhase::CANCELLED);
	for (const auto& unit : h.machine.Units()) EXPECT_TRUE(IsFinished(unit.state));
}

TEST(LiveUpdateTest, ShuttingDownInstancesAreLeftOut) {
	Harness h;
	auto stopping = World(1100, 1, 0);
	stopping.shuttingDown = true;
	h.master.Add(stopping);
	h.master.Add(World(1200, 2, 1));
	h.Start();
	size_t worlds = 0;
	for (const auto& unit : h.machine.Units()) if (unit.kind == eUnitKind::WORLD) worlds++;
	EXPECT_EQ(worlds, 1u);
}

TEST(LiveUpdateTest, StatusCarriesEveryUnit) {
	Harness h;
	h.master.Add(World(1150, 4, 2));
	h.Start();
	LiveUpdateStatus status;
	h.machine.FillStatus(status);
	status.updateId = 3;
	status.by = "Aaron";
	status.startedAt = 1700000000;
	ASSERT_EQ(status.units.size(), 6u);

	RakNet::BitStream stream;
	status.Serialize(stream);
	LiveUpdateStatus read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.updateId, 3u);
	EXPECT_EQ(read.phase, ePhase::RUNNING);
	EXPECT_EQ(read.by, "Aaron");
	EXPECT_EQ(read.startedAt, 1700000000);
	ASSERT_EQ(read.units.size(), 6u);
	EXPECT_EQ(read.units[0].kind, eUnitKind::DATABASE);
	EXPECT_EQ(read.units[4].kind, eUnitKind::WORLD);
	EXPECT_EQ(read.units[4].zoneId, 1150u);
	EXPECT_EQ(read.units[4].instanceId, 4u);
	EXPECT_EQ(read.units[4].cloneId, 0u);
	EXPECT_EQ(read.units[4].players, 2u);
	EXPECT_EQ(read.units[5].kind, eUnitKind::DASHBOARD);
}

TEST(LiveUpdateTest, RequestRoundTripAndLimits) {
	LiveUpdateRequest request;
	request.action = eAction::START;
	request.warnSeconds = 30;
	request.requesterId = 1152921510436607007LL;
	request.requestedBy = "Aaron";
	RakNet::BitStream stream;
	request.Serialize(stream);
	LiveUpdateRequest read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.action, eAction::START);
	EXPECT_EQ(read.warnSeconds, 30);
	EXPECT_EQ(read.requesterId, 1152921510436607007LL);
	EXPECT_EQ(read.requestedBy, "Aaron");

	RakNet::BitStream bad;
	bad.Write<uint8_t>(9);
	EXPECT_FALSE(read.Deserialize(bad));
	RakNet::BitStream tooLong;
	tooLong.Write<uint8_t>(0);
	tooLong.Write<int32_t>(301);
	tooLong.Write<LWOOBJID>(0);
	tooLong.Write<uint16_t>(0);
	EXPECT_FALSE(read.Deserialize(tooLong));
}

TEST(LiveUpdateTest, ChatHandoffRoundTrip) {
	ChatHandoff handoff;
	auto& team = handoff.teams.emplace_back();
	team.teamId = 7;
	team.leaderId = 100;
	team.members = { 100, 101, 102 };
	team.lootFlag = 1;
	team.local = true;
	team.zoneId = 1100;
	team.instanceId = 4;
	team.cloneId = 0;
	RakNet::BitStream stream;
	handoff.Serialize(stream);
	ChatHandoff read;
	ASSERT_TRUE(read.Deserialize(stream));
	ASSERT_EQ(read.teams.size(), 1u);
	EXPECT_EQ(read.teams[0].teamId, 7);
	EXPECT_EQ(read.teams[0].leaderId, 100);
	EXPECT_EQ(read.teams[0].members, (std::vector<LWOOBJID>{ 100, 101, 102 }));
	EXPECT_EQ(read.teams[0].lootFlag, 1u);
	EXPECT_TRUE(read.teams[0].local);
	EXPECT_EQ(read.teams[0].zoneId, 1100u);
	EXPECT_EQ(read.teams[0].instanceId, 4u);
}

TEST(LiveUpdateTest, CarriedPositionIsOptional) {
	CarriedPlayerState state;
	state.targetZone = 1150;
	state.targetInstance = 3;
	state.characterId = 5;
	state.hasPosition = true;
	state.x = 1.5f;
	state.y = -2.0f;
	state.z = 300.25f;
	state.rotW = 0.5f;
	state.rotY = 0.5f;
	RakNet::BitStream stream;
	state.Serialize(stream);
	CarriedPlayerState read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_TRUE(read.hasPosition);
	EXPECT_FLOAT_EQ(read.x, 1.5f);
	EXPECT_FLOAT_EQ(read.y, -2.0f);
	EXPECT_FLOAT_EQ(read.z, 300.25f);
	EXPECT_FLOAT_EQ(read.rotW, 0.5f);
	EXPECT_FLOAT_EQ(read.rotY, 0.5f);

	// As an older world wrote it: no position at all
	RakNet::BitStream old;
	old.Write<uint32_t>(1150);
	old.Write<uint32_t>(3);
	old.Write<LWOOBJID>(5);
	old.Write<LWOOBJID>(0);
	old.Write<uint8_t>(0);
	CarriedPlayerState oldRead;
	ASSERT_TRUE(oldRead.Deserialize(old));
	EXPECT_FALSE(oldRead.hasPosition);
	EXPECT_EQ(oldRead.characterId, 5);
}

TEST(LiveUpdateTest, OrderWaitIsOptional) {
	MigratePlayersOrder order;
	order.migrationId = 1;
	order.targetIp = "localhost";
	order.targetPort = 3000;
	order.maxWaitSeconds = 45;
	RakNet::BitStream stream;
	order.Serialize(stream);
	MigratePlayersOrder read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.maxWaitSeconds, 45u);

	// Without it (an older master): the default
	RakNet::BitStream old;
	old.Write<uint32_t>(1);
	old.Write<uint32_t>(1100);
	old.Write<uint32_t>(2);
	old.Write<uint32_t>(0);
	InstanceMigration::WriteText(old, "localhost", MigratePlayersOrder::MAX_IP);
	old.Write<uint16_t>(3000);
	old.Write<uint16_t>(10);
	old.Write<uint16_t>(10);
	old.Write<uint8_t>(1);
	old.Write<uint8_t>(0);
	MigratePlayersOrder oldRead;
	ASSERT_TRUE(oldRead.Deserialize(old));
	EXPECT_EQ(oldRead.maxWaitSeconds, MigratePlayersOrder::DEFAULT_MAX_WAIT_SECONDS);
}

TEST(LiveUpdateTest, PrepareAndPreparedStatus) {
	MigratePrepare prepare;
	prepare.migrationId = 9;
	prepare.maxWaitSeconds = 90;
	RakNet::BitStream stream;
	prepare.Serialize(stream);
	MigratePrepare read;
	ASSERT_TRUE(read.Deserialize(stream));
	EXPECT_EQ(read.migrationId, 9u);
	EXPECT_EQ(read.maxWaitSeconds, 90u);

	MigrationStatus status;
	status.state = InstanceMigration::eState::PREPARED;
	RakNet::BitStream statusStream;
	status.Serialize(statusStream);
	MigrationStatus statusRead;
	ASSERT_TRUE(statusRead.Deserialize(statusStream));
	EXPECT_EQ(statusRead.state, InstanceMigration::eState::PREPARED);
	EXPECT_FALSE(statusRead.Finished());
}
