#ifndef WORLDRELOADER_H
#define WORLDRELOADER_H

#include <functional>
#include <string>

#include "RakNetTypes.h"

class Instance;
struct MigrationStatus;
struct WorldFilesReport;
struct WorldFilesStatus;
struct WorldReloadRequest;

/**
 * Master's side of world hot reload (docs/WorldHotReload.md). Worlds report the zone files they loaded (WORLD_FILES);
 * master watches them (size and mtime every world_watch_seconds, then a hash on a worker thread) and, when one changes
 * or a GM or the dashboard asks (WORLD_RELOAD), replaces the affected instances with new ones on the files on disk,
 * moving their players with the instance migration (MigrationCoordinator). Main thread only.
 */
namespace WorldReloader {
	// Where the dashboard's status goes (WORLD_FILES_STATUS)
	void SetPublisher(std::function<void(const WorldFilesStatus&)> publisher);

	// A world reported its files
	void HandleReport(const SystemAddress& from, const WorldFilesReport& report);

	// A GM's /reloadworld or the dashboard; who is for the log
	void HandleRequest(const WorldReloadRequest& request, const std::string& who);

	// Every migration's progress (MigrationCoordinator's observer)
	void OnMigrationStatus(const MigrationStatus& status);

	// A world server went away
	void OnInstanceGone(const Instance& instance);

	// The dashboard connected: send it the status again
	void Republish();

	// Every master frame: polls the files, finishes hashes and reloads what changed
	void Update();

	// Waits for a running hash (at shutdown)
	void Shutdown();
};

#endif // WORLDRELOADER_H
