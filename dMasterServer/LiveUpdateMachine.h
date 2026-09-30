#ifndef __LIVEUPDATEMACHINE__H__
#define __LIVEUPDATEMACHINE__H__

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "master/InstanceMigration.h"
#include "master/LiveUpdate.h"

/**
 * The order of a live update (docs/LiveUpdate.md), without master's state: it sees the servers and instances master
 * knows (Observed), asks for things to be done through IActions, and hears how instance migrations go (OnMigration).
 * Master's LiveUpdateCoordinator runs it every frame. Header only, so it is unit tested without a master server.
 *
 * Order: database migrations; then the UGC server, auth and chat together; once chat is back, the world instances
 * (a few at a time); the dashboard last, so it shows the progress until then.
 */
namespace LiveUpdate {
	using Clock = std::chrono::steady_clock;

	enum class eService : uint8_t { UGC, AUTH, CHAT, DASHBOARD };

	inline const char* ServiceName(eService service) {
		switch (service) {
		case eService::UGC: return "UGC server";
		case eService::AUTH: return "auth server";
		case eService::CHAT: return "chat server";
		case eService::DASHBOARD: return "dashboard";
		}
		return "server";
	}

	struct Settings {
		uint16_t warnSeconds{ 10 };                       // players are warned this long before they are moved
		uint32_t parallelWorlds{ 4 };                     // world instances being replaced at once
		std::chrono::seconds serviceTimeout{ 60 };        // a server to stop, or its new process to connect
		std::chrono::seconds ugcDrainTimeout{ 300 };      // the UGC server finishing the jobs it is running
		std::chrono::seconds charSelectWait{ 60 };        // players at character selection to pick a character
		std::chrono::seconds activityWait{ 1800 };        // players in an activity zone (races, minigames) to finish
		std::chrono::seconds worldStartTimeout{ 180 };    // a replacement instance to be ready
		std::chrono::seconds worldTimeout{ 900 };         // backstop for one world's whole replacement
		std::chrono::seconds chatSettle{ 3 };             // worlds to reconnect to the new chat server
		uint16_t playerWaitSeconds{ 30 };                 // dead or building players before they are moved anyway
		uint16_t propertyBuildWaitSeconds{ 60 };          // builders on a property before it is saved anyway
		bool runMigrations{ true };
		std::set<uint32_t> keepZones{ 0, 1000 };          // zones that always have an instance (prestart_worlds)
		uint32_t maxServiceStarts{ 3 };                   // tries to start a server that didn't come back
	};

	struct ServiceView {
		bool enabled{ true };
		bool online{};
		uint32_t connects{}; // counts every time one connected to master
	};

	struct WorldView {
		InstanceMigration::InstanceView view;
		bool activityZone{};
	};

	struct Observed {
		ServiceView ugc, auth, chat, dashboard;
		std::vector<WorldView> worlds;

		const WorldView* Find(uint32_t zone, uint32_t instance) const {
			for (const auto& world : worlds) {
				if (world.view.zoneId == zone && world.view.instanceId == instance) return &world;
			}
			return nullptr;
		}

		const ServiceView& Service(eService service) const {
			switch (service) {
			case eService::UGC: return ugc;
			case eService::AUTH: return auth;
			case eService::CHAT: return chat;
			case eService::DASHBOARD: return dashboard;
			}
			return auth;
		}
	};

	// What the machine has master do
	class IActions {
	public:
		virtual ~IActions() = default;
		// The new build's database migrations; false (and why) when they failed
		virtual bool RunMigrations(std::string& error) = 0;
		// Chat and UGC: LIVE_UPDATE_RETIRE; auth and the dashboard: SHUTDOWN. Master starts the new one when it disconnects.
		virtual void RetireService(eService service) = 0;
		// SHUTDOWN right away (a UGC server that took too long to finish its jobs)
		virtual void StopService(eService service) = 0;
		virtual void StartService(eService service) = 0;
		// CHAT_SERVER_READY to every world
		virtual void ChatReady() = 0;
		// Start a new public instance of zone (clone 0); its instance ID, or none when master refused
		virtual std::optional<uint32_t> StartWorld(uint32_t zone) = 0;
		virtual void SetDraining(uint32_t zone, uint32_t instance, bool draining) = 0;

		struct Move {
			uint32_t zone{};
			uint32_t instance{};
			InstanceMigration::eKind kind{ InstanceMigration::eKind::REPLACE };
			uint32_t target{};         // merges only
			uint16_t warnSeconds{};
			uint16_t playerWaitSeconds{};
			bool prepare{};            // properties: freeze and save before the new instance loads it
			uint16_t prepareWaitSeconds{};
		};
		// An instance migration (MigrationCoordinator); its ID, or none (and why) when refused
		virtual std::optional<uint32_t> Migrate(const Move& move, std::string& refusal) = 0;
		virtual void StopWorld(uint32_t zone, uint32_t instance) = 0;
	};

	// What happens to one world instance, decided when its turn comes
	enum class eWorldPlan : uint8_t {
		STOP,           // nobody there: shut it down (a new one starts when someone goes there)
		REPLACE_EMPTY,  // nobody there, but the zone should always have one: start the new one, then stop this one
		MOVE,           // replace it and move its players (properties are never planned: InReplacePlan)
		WAIT_THEN_MOVE, // activity zones and character selection: nobody new goes there; its players leave by
		                // themselves or, after a while, are moved
	};

	inline const char* PlanName(eWorldPlan plan) {
		switch (plan) {
		case eWorldPlan::STOP: return "stop";
		case eWorldPlan::REPLACE_EMPTY: return "replace_empty";
		case eWorldPlan::MOVE: return "move";
		case eWorldPlan::WAIT_THEN_MOVE: return "wait_then_move";
		}
		return "unknown";
	}

	// zoneReplaced: this update already started a new instance of the zone (keeping one is then taken care of)
	inline eWorldPlan PlanWorld(const InstanceMigration::InstanceView& view, bool activityZone, bool keepZone, bool zoneReplaced) {
		if (view.players <= 0) {
			if ((keepZone || view.zoneId == 0) && !view.isPrivate && view.cloneId == 0 && !zoneReplaced) return eWorldPlan::REPLACE_EMPTY;
			return eWorldPlan::STOP;
		}
		if (view.zoneId == 0 || activityZone) return eWorldPlan::WAIT_THEN_MOVE;
		return eWorldPlan::MOVE;
	}

	/**
	 * Which running instances a live update replaces: all but those already shutting down and properties. A property
	 * (clone) is never moved: building in progress there isn't saved. It is marked outdated like every other instance
	 * (master: nobody new goes there) and stops by itself once everyone left (OutdatedInstances.h).
	 */
	inline bool InReplacePlan(const InstanceMigration::InstanceView& view) {
		return !view.shuttingDown && view.cloneId == 0;
	}


	struct Unit {
		eUnitKind kind{};
		eUnitState state{ eUnitState::PENDING };
		eWorldPlan plan{ eWorldPlan::STOP };
		uint32_t zone{};
		uint32_t instance{};
		uint32_t clone{};
		bool isPrivate{};
		uint32_t replacement{};
		uint32_t players{};
		uint16_t moved{};
		uint32_t migrationId{};
		bool drained{};         // we set it draining (so we undo that if it fails)
		uint32_t connectsAtStart{};
		uint32_t starts{};
		Clock::time_point deadline{};
		std::string message;
	};

	class Machine {
	public:
		/**
		 * Plans an update of what runs now. False (and why) when one is already running. Instances that are shutting
		 * down are left out; instances started later are the new build's already.
		 */
		bool Start(const Settings& settings, const Observed& observed, Clock::time_point now, std::string& error) {
			if (m_Phase == ePhase::RUNNING || m_Phase == ePhase::CANCELLING) {
				error = "A live update is already running";
				return false;
			}
			m_Settings = settings;
			m_Units.clear();
			m_ReplacedZones.clear();
			m_Phase = ePhase::RUNNING;
			m_Message.clear();
			m_Dirty = true;
			m_Units.push_back(ServiceUnit(eUnitKind::DATABASE));
			m_Units.push_back(ServiceUnit(eUnitKind::UGC));
			m_Units.push_back(ServiceUnit(eUnitKind::AUTH));
			m_Units.push_back(ServiceUnit(eUnitKind::CHAT));
			// Character selection first (logins go to the new one soonest), then the busiest
			std::vector<WorldView> worlds;
			for (const auto& world : observed.worlds) {
				if (InReplacePlan(world.view)) worlds.push_back(world);
			}
			std::stable_sort(worlds.begin(), worlds.end(), [](const WorldView& a, const WorldView& b) {
				if ((a.view.zoneId == 0) != (b.view.zoneId == 0)) return a.view.zoneId == 0;
				return a.view.players > b.view.players;
			});
			for (const auto& world : worlds) {
				Unit unit;
				unit.kind = eUnitKind::WORLD;
				unit.zone = world.view.zoneId;
				unit.instance = world.view.instanceId;
				unit.clone = world.view.cloneId;
				unit.isPrivate = world.view.isPrivate;
				unit.players = static_cast<uint32_t>(std::max(world.view.players, 0));
				m_Units.push_back(unit);
			}
			m_Units.push_back(ServiceUnit(eUnitKind::DASHBOARD));
			(void)now;
			return true;
		}

		// Start nothing new; what is under way finishes. False when nothing is running.
		bool Cancel() {
			if (m_Phase != ePhase::RUNNING) return false;
			m_Phase = ePhase::CANCELLING;
			for (auto& unit : m_Units) {
				if (unit.state == eUnitState::PENDING) SetState(unit, eUnitState::SKIPPED, "Cancelled");
			}
			m_Message = "Cancelled; waiting for what is under way";
			m_Dirty = true;
			return true;
		}

		// Master is shutting down: forget the rest
		void Abort(const std::string& why) {
			if (!IsRunning()) return;
			for (auto& unit : m_Units) {
				if (!IsFinished(unit.state)) SetState(unit, eUnitState::SKIPPED, why);
			}
			Finish(ePhase::CANCELLED, why);
		}

		/**
		 * Moves things along. Master calls it every frame (with what it knows now); it returns whether anything changed
		 * (the status is sent again then).
		 */
		bool Tick(const Observed& observed, Clock::time_point now, IActions& actions) {
			if (!IsRunning()) return TakeDirty();

			auto& database = m_Units[0];
			if (database.state == eUnitState::PENDING) {
				if (!m_Settings.runMigrations) {
					SetState(database, eUnitState::SKIPPED, "Switched off (live_update_run_migrations)");
				} else {
					std::string error;
					if (actions.RunMigrations(error)) {
						SetState(database, eUnitState::STOPPED, "Up to date");
					} else {
						SetState(database, eUnitState::FAILED, error.empty() ? "The migrations failed" : error);
						for (auto& unit : m_Units) {
							if (unit.state == eUnitState::PENDING) SetState(unit, eUnitState::SKIPPED, "Not started: the database migrations failed");
						}
						Finish(ePhase::FAILED, "The database migrations failed; nothing else was touched");
						return TakeDirty();
					}
				}
			}

			for (auto& unit : m_Units) {
				switch (unit.kind) {
				case eUnitKind::UGC: TickService(unit, eService::UGC, observed, now, actions); break;
				case eUnitKind::AUTH: TickService(unit, eService::AUTH, observed, now, actions); break;
				case eUnitKind::CHAT: TickService(unit, eService::CHAT, observed, now, actions); break;
				default: break;
				}
			}

			// Worlds once chat is back (their players are sent to the new chat server first)
			if (IsFinished(Find(eUnitKind::CHAT).state)) {
				for (auto& unit : m_Units) {
					if (unit.kind != eUnitKind::WORLD || IsFinished(unit.state)) continue;
					if (unit.state == eUnitState::PENDING) {
						if (m_Phase != ePhase::RUNNING || Busy() >= std::max<uint32_t>(m_Settings.parallelWorlds, 1)) continue;
						StartWorld(unit, observed, now, actions);
					} else {
						TickWorld(unit, observed, now, actions);
					}
				}
			}

			// The dashboard last: it shows the progress until then
			auto& dashboard = Find(eUnitKind::DASHBOARD);
			const bool othersDone = std::all_of(m_Units.begin(), m_Units.end(), [](const Unit& unit) {
				return unit.kind == eUnitKind::DASHBOARD || IsFinished(unit.state);
			});
			if (othersDone && (dashboard.state != eUnitState::PENDING || m_Phase == ePhase::RUNNING)) {
				TickService(dashboard, eService::DASHBOARD, observed, now, actions);
			}

			if (std::all_of(m_Units.begin(), m_Units.end(), [](const Unit& unit) { return IsFinished(unit.state); })) {
				const auto failed = std::count_if(m_Units.begin(), m_Units.end(), [](const Unit& unit) { return unit.state == eUnitState::FAILED; });
				if (m_Phase == ePhase::CANCELLING) Finish(ePhase::CANCELLED, "Cancelled");
				else Finish(ePhase::DONE, failed == 0 ? "Everything runs on the new build" :
					std::to_string(failed) + " part(s) failed and still run on the old build");
			}
			return TakeDirty();
		}

		// How an instance migration this update started is going
		void OnMigration(const MigrationStatus& status) {
			for (auto& unit : m_Units) {
				if (unit.kind != eUnitKind::WORLD || unit.migrationId == 0 || unit.migrationId != status.migrationId || IsFinished(unit.state)) continue;
				using InstanceMigration::eState;
				if (status.targetInstance != 0) unit.replacement = status.targetInstance;
				unit.moved = status.moved;
				switch (status.state) {
				case eState::PREPARING: SetState(unit, eUnitState::PREPARING, "Saving the property"); break;
				case eState::PREPARED: SetState(unit, eUnitState::STARTING, "Property saved"); break;
				case eState::STARTING_TARGET: SetState(unit, eUnitState::STARTING, "Starting instance " + std::to_string(unit.replacement)); break;
				case eState::WARNING: SetState(unit, eUnitState::READY, "Instance " + std::to_string(unit.replacement) + " is ready; players were warned"); break;
				case eState::MOVING: SetState(unit, eUnitState::DRAINING, std::to_string(status.moved) + " moved, " + std::to_string(status.remaining) + " left"); break;
				case eState::DONE:
					unit.migrationId = 0;
					SetState(unit, eUnitState::STOPPING, std::to_string(status.moved) + " player(s) moved" + (status.message.empty() ? "" : "; " + status.message));
					unit.deadline = Clock::time_point::max(); // TickWorld sets the stop deadline
					break;
				case eState::FAILED:
					unit.migrationId = 0;
					SetState(unit, eUnitState::FAILED, status.message.empty() ? "The move failed" : status.message);
					break;
				}
				m_Dirty = true;
			}
		}

		bool IsRunning() const { return m_Phase == ePhase::RUNNING || m_Phase == ePhase::CANCELLING; }
		ePhase Phase() const { return m_Phase; }
		const std::vector<Unit>& Units() const { return m_Units; }
		const std::string& Message() const { return m_Message; }
		const Settings& GetSettings() const { return m_Settings; }

		// Units taking a slot of parallelWorlds: worlds being prepared, started or emptied
		uint32_t Busy() const {
			uint32_t busy = 0;
			for (const auto& unit : m_Units) {
				if (unit.kind != eUnitKind::WORLD) continue;
				if (unit.state == eUnitState::PREPARING || unit.state == eUnitState::STARTING || unit.state == eUnitState::READY ||
					unit.state == eUnitState::DRAINING) busy++;
			}
			return busy;
		}

		void FillStatus(LiveUpdateStatus& status) const {
			status.phase = m_Phase;
			status.message = m_Message;
			status.units.clear();
			for (const auto& unit : m_Units) {
				LiveUpdateStatus::Unit row;
				row.kind = unit.kind;
				row.state = unit.state;
				row.zoneId = unit.zone;
				row.instanceId = unit.instance;
				row.cloneId = unit.clone;
				row.replacement = unit.replacement;
				row.players = unit.players;
				row.moved = unit.moved;
				row.isPrivate = unit.isPrivate ? 1 : 0;
				row.message = unit.message;
				status.units.push_back(std::move(row));
			}
		}

	private:
		static Unit ServiceUnit(eUnitKind kind) {
			Unit unit;
			unit.kind = kind;
			return unit;
		}

		Unit& Find(eUnitKind kind) {
			for (auto& unit : m_Units) {
				if (unit.kind == kind) return unit;
			}
			return m_Units.front();
		}

		void SetState(Unit& unit, eUnitState state, const std::string& message) {
			if (unit.state != state || unit.message != message) m_Dirty = true;
			unit.state = state;
			unit.message = message;
		}

		void Finish(ePhase phase, const std::string& message) {
			m_Phase = phase;
			m_Message = message;
			m_Dirty = true;
		}

		bool TakeDirty() {
			const bool dirty = m_Dirty;
			m_Dirty = false;
			return dirty;
		}

		// A server: asked to stop (the UGC server first finishes what it is making); master starts the new one when the
		// old one disconnects; done once a new one connected
		void TickService(Unit& unit, eService service, const Observed& observed, Clock::time_point now, IActions& actions) {
			const auto& view = observed.Service(service);
			const bool replaced = view.connects > unit.connectsAtStart && view.online;
			switch (unit.state) {
			case eUnitState::PENDING:
				if (m_Phase != ePhase::RUNNING) return SetState(unit, eUnitState::SKIPPED, "Cancelled");
				if (!view.enabled) return SetState(unit, eUnitState::SKIPPED, "Not enabled");
				if (!view.online) return SetState(unit, eUnitState::SKIPPED, "Not running (master starts it from the new build when it comes back)");
				unit.connectsAtStart = view.connects;
				actions.RetireService(service);
				if (service == eService::UGC) {
					unit.deadline = now + m_Settings.ugcDrainTimeout;
					return SetState(unit, eUnitState::DRAINING, "Finishing the jobs it is running");
				}
				unit.deadline = now + m_Settings.serviceTimeout;
				return SetState(unit, eUnitState::STOPPING, service == eService::CHAT ? "Handing its teams over" : "Stopping");
			case eUnitState::DRAINING:
			case eUnitState::STOPPING:
				if (replaced) return Replaced(unit, service, now, actions);
				if (!view.online) {
					unit.deadline = now + m_Settings.serviceTimeout;
					return SetState(unit, eUnitState::STARTING, "Starting the new " + std::string(ServiceName(service)));
				}
				if (now < unit.deadline) return;
				if (unit.state == eUnitState::DRAINING) {
					actions.StopService(service);
					unit.deadline = now + m_Settings.serviceTimeout;
					return SetState(unit, eUnitState::STOPPING, "Took too long; stopped (the jobs it was making are made again)");
				}
				return SetState(unit, eUnitState::FAILED, "It didn't stop; the old one keeps running");
			case eUnitState::STARTING:
				if (replaced) return Replaced(unit, service, now, actions);
				if (now < unit.deadline) return;
				if (unit.starts >= m_Settings.maxServiceStarts) {
					return SetState(unit, eUnitState::FAILED, "The new " + std::string(ServiceName(service)) + " didn't come up");
				}
				unit.starts++;
				actions.StartService(service);
				unit.deadline = now + m_Settings.serviceTimeout;
				return SetState(unit, eUnitState::STARTING, "Starting the new " + std::string(ServiceName(service)) + " again (try " + std::to_string(unit.starts) + ")");
			case eUnitState::READY:
				// Chat: worlds had a moment to reconnect and send their players
				if (now >= unit.deadline) SetState(unit, eUnitState::STOPPED, "Replaced; worlds sent it their players");
				return;
			default:
				return;
			}
		}

		void Replaced(Unit& unit, eService service, Clock::time_point now, IActions& actions) {
			if (service != eService::CHAT) return SetState(unit, eUnitState::STOPPED, "Replaced");
			actions.ChatReady();
			unit.deadline = now + m_Settings.chatSettle;
			SetState(unit, eUnitState::READY, "New chat server is up; worlds are reconnecting");
		}

		IActions::Move MoveFor(const Unit& unit, InstanceMigration::eKind kind, uint32_t target) const {
			IActions::Move move;
			move.zone = unit.zone;
			move.instance = unit.instance;
			move.kind = kind;
			move.target = target;
			move.warnSeconds = m_Settings.warnSeconds;
			move.playerWaitSeconds = m_Settings.playerWaitSeconds;
			move.prepare = unit.clone != 0;
			move.prepareWaitSeconds = m_Settings.propertyBuildWaitSeconds;
			return move;
		}

		void StartMigration(Unit& unit, InstanceMigration::eKind kind, uint32_t target, Clock::time_point now, IActions& actions) {
			std::string refusal;
			const auto id = actions.Migrate(MoveFor(unit, kind, target), refusal);
			if (!id) {
				if (unit.drained) actions.SetDraining(unit.zone, unit.instance, false);
				return SetState(unit, eUnitState::FAILED, refusal.empty() ? "Master refused to move its players" : refusal);
			}
			unit.migrationId = *id;
			unit.deadline = now + m_Settings.worldTimeout;
			if (unit.clone != 0) SetState(unit, eUnitState::PREPARING, "Saving the property");
			else SetState(unit, eUnitState::STARTING, "Starting its replacement");
		}

		void StopOld(Unit& unit, Clock::time_point now, IActions& actions, const std::string& message) {
			if (!unit.drained) {
				actions.SetDraining(unit.zone, unit.instance, true);
				unit.drained = true;
			}
			actions.StopWorld(unit.zone, unit.instance);
			unit.deadline = now + m_Settings.serviceTimeout;
			SetState(unit, eUnitState::STOPPING, message);
		}

		// Whether zone has a public instance on the new build already: one started after the update began (not in the
		// plan), running and taking players
		bool HasNewInstance(const Observed& observed, uint32_t zone) const {
			return std::any_of(observed.worlds.begin(), observed.worlds.end(), [&](const WorldView& world) {
				const auto& view = world.view;
				if (view.zoneId != zone || view.cloneId != 0 || view.isPrivate || view.outdated || view.shuttingDown || view.draining) return false;
				return std::none_of(m_Units.begin(), m_Units.end(), [&](const Unit& unit) {
					return unit.kind == eUnitKind::WORLD && unit.zone == view.zoneId && unit.instance == view.instanceId;
				});
			});
		}

		void StartWorld(Unit& unit, const Observed& observed, Clock::time_point now, IActions& actions) {
			const auto* world = observed.Find(unit.zone, unit.instance);
			if (!world || world->view.shuttingDown) return SetState(unit, eUnitState::STOPPED, "It stopped by itself");
			unit.players = static_cast<uint32_t>(std::max(world->view.players, 0));
			// A zone somebody went to since the update began has its new instance already
			if (HasNewInstance(observed, unit.zone)) m_ReplacedZones.insert(unit.zone);
			unit.plan = PlanWorld(world->view, world->activityZone, m_Settings.keepZones.contains(unit.zone), m_ReplacedZones.contains(unit.zone));
			switch (unit.plan) {
			case eWorldPlan::STOP:
				return StopOld(unit, now, actions, "Nobody there; stopping it");
			case eWorldPlan::REPLACE_EMPTY: {
				const auto started = actions.StartWorld(unit.zone);
				if (!started) return SetState(unit, eUnitState::FAILED, "Master refused to start a new instance");
				unit.replacement = *started;
				m_ReplacedZones.insert(unit.zone);
				unit.deadline = now + m_Settings.worldStartTimeout;
				return SetState(unit, eUnitState::STARTING, "Starting instance " + std::to_string(unit.replacement));
			}
			case eWorldPlan::MOVE:
				return StartMigration(unit, InstanceMigration::eKind::REPLACE, 0, now, actions);
			case eWorldPlan::WAIT_THEN_MOVE: {
				// Character selection: logins go to a new one at once
				if (unit.zone == 0 && !m_ReplacedZones.contains(0)) {
					if (const auto started = actions.StartWorld(0)) {
						unit.replacement = *started;
						m_ReplacedZones.insert(0);
					}
				}
				actions.SetDraining(unit.zone, unit.instance, true);
				unit.drained = true;
				unit.deadline = now + (unit.zone == 0 ? m_Settings.charSelectWait : m_Settings.activityWait);
				return SetState(unit, eUnitState::WAITING, unit.zone == 0 ? "Waiting for players to pick a character" : "Waiting for the activity to end");
			}
			}
		}

		void TickWorld(Unit& unit, const Observed& observed, Clock::time_point now, IActions& actions) {
			const auto* world = observed.Find(unit.zone, unit.instance);
			switch (unit.state) {
			case eUnitState::STOPPING:
				if (!world) return SetState(unit, eUnitState::STOPPED, unit.message.empty() ? "Stopped" : unit.message);
				if (unit.deadline == Clock::time_point::max()) unit.deadline = now + m_Settings.worldStartTimeout;
				if (now >= unit.deadline) SetState(unit, eUnitState::FAILED, "It didn't stop");
				return;
			case eUnitState::WAITING: {
				if (!world) return SetState(unit, eUnitState::STOPPED, "Everyone left; it stopped");
				if (world->view.players <= 0) return StopOld(unit, now, actions, "Everyone left; stopping it");
				if (now < unit.deadline) return;
				// Whoever is still there is moved: from character selection into the new one, from an activity into a
				// fresh instance (the activity is lost)
				const auto* replacement = unit.replacement ? observed.Find(unit.zone, unit.replacement) : nullptr;
				if (replacement && replacement->view.ready) return StartMigration(unit, InstanceMigration::eKind::MERGE, unit.replacement, now, actions);
				return StartMigration(unit, InstanceMigration::eKind::REPLACE, 0, now, actions);
			}
			case eUnitState::STARTING:
			case eUnitState::READY:
				if (unit.plan == eWorldPlan::REPLACE_EMPTY) {
					const auto* replacement = observed.Find(unit.zone, unit.replacement);
					if (!world) return SetState(unit, eUnitState::STOPPED, "It stopped by itself");
					if (!replacement) return SetState(unit, eUnitState::FAILED, "Instance " + std::to_string(unit.replacement) + " stopped while starting");
					if (!replacement->view.ready) {
						if (now >= unit.deadline) SetState(unit, eUnitState::FAILED, "Instance " + std::to_string(unit.replacement) + " didn't start in time");
						return;
					}
					// Someone may have arrived before it was ready: they are moved over
					if (world->view.players > 0) {
						actions.SetDraining(unit.zone, unit.instance, true);
						unit.drained = true;
						return StartMigration(unit, InstanceMigration::eKind::MERGE, unit.replacement, now, actions);
					}
					return StopOld(unit, now, actions, "Instance " + std::to_string(unit.replacement) + " took over; stopping it");
				}
				[[fallthrough]];
			case eUnitState::PREPARING:
			case eUnitState::DRAINING:
				// The migration reports how it goes (OnMigration); this is only a backstop
				if (now >= unit.deadline) SetState(unit, eUnitState::FAILED, "Timed out");
				return;
			default:
				return;
			}
		}

		Settings m_Settings;
		ePhase m_Phase{ ePhase::IDLE };
		std::vector<Unit> m_Units;
		std::set<uint32_t> m_ReplacedZones;
		std::string m_Message;
		bool m_Dirty{};
	};
}

#endif  //!__LIVEUPDATEMACHINE__H__
