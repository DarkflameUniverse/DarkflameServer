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

	// A migration was asked for (a GM command, or later a dashboard); anything but NONE means it was refused (and
	// reported as FAILED)
	InstanceMigration::eRefusal Start(const InstanceMigrationRequest& request);

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
