#ifndef __MIGRATIONCOORDINATOR__H__
#define __MIGRATIONCOORDINATOR__H__

#include <functional>

#include "master/InstanceMigration.h"
#include "RakNetTypes.h"

class Instance;

/**
 * Master's side of instance migrations (InstanceMigration.h): picks or starts the target instance, holds seats
 * there, tells the source world to send its players over and passes progress on to whoever asked. The source
 * instance is "draining" meanwhile, so no new player is sent to it.
 */
namespace MigrationCoordinator {
	// Where statuses go (every world, so the GM who asked hears about it wherever they are); set once at startup
	void SetReporter(std::function<void(const MigrationStatus&)> reporter);

	// Also told every status, including those nobody asked for in game (live updates follow their migrations here)
	void SetObserver(std::function<void(const MigrationStatus&)> observer);

	// What a live update (LiveUpdateCoordinator) changes about a migration
	struct Options {
		// Moves what CheckSource refuses (see CheckLiveUpdateSource): character selection, private instances,
		// properties and activity zones
		bool liveUpdate{};
		// Properties: the source saves and freezes the property (MIGRATE_PREPARE) before the new instance is started
		bool prepare{};
		uint16_t prepareWaitSeconds{ 60 };
		// Players who are dead or building wait this long before they are moved anyway
		uint16_t playerWaitSeconds{ MigratePlayersOrder::DEFAULT_MAX_WAIT_SECONDS };
	};

	// A migration was asked for (a GM command, or a live update); anything but NONE means it was refused (and
	// reported as FAILED)
	InstanceMigration::eRefusal Start(const InstanceMigrationRequest& request, const Options& options = {});

	// A world reported progress on a migration it is running
	void HandleStatus(const SystemAddress& from, const MigrationStatus& status);

	// A source world sent state to carry over for one player: passed on to the target world as is
	void HandleCarriedState(const SystemAddress& from, const CarriedPlayerState& state);

	// A world server went away; migrations it was part of end
	void OnInstanceGone(const Instance& instance);

	// Every master frame: waits for targets to be ready and times out stuck migrations
	void Update();
}

#endif  //!__MIGRATIONCOORDINATOR__H__
