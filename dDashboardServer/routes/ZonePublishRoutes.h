#pragma once

#include <cstdint>

#include "json.hpp"

struct WorldFilesStatus;

/**
 * The zone file API (/api/v1/zones/..., docs/ZonePublish.md): an external level editor sends a zone's edited files,
 * which are checked, kept in the override folder and loaded by replacing the zone's instances. A thin layer: who
 * may is ZonePublishAuth (the key) and ZoneEditLock (the zone's lock), checking a set is ZonePublish, keeping it is
 * ZoneOverrideStore, and the reload is the world reload master already does (WorldReloader).
 *
 * Routes and their database use run on the main thread; reading and writing the files runs on one worker thread,
 * which touches nothing else and hands its result back to the main thread (Update).
 */
namespace ZonePublishRoutes {
	void RegisterRoutes();

	// Master's world file status (WORLD_FILES_STATUS): how far the reloads asked for here have come
	void HandleStatus(const WorldFilesStatus& status);

	// Main loop: finish what the worker did
	void Update();

	// Stops the worker (a set being written finishes first)
	void Shutdown();

	// What master counts per zone (WorldFilesStatus::Zone); present: the zone has instances that reported files
	struct ReloadCounters {
		bool present{};
		uint32_t requests{};
		uint32_t acted{};
		uint32_t playersMoved{};
		uint32_t failed{};
		uint16_t pending{};
	};

	struct ReloadProgress {
		bool done{};
		uint32_t instances{};
		uint32_t playersMoved{};
		uint32_t failed{};
	};

	/**
	 * What a reload asked for when the counters were `before` has done by `now`: done once master took the request
	 * and nothing of the zone is being replaced any more, or the zone has no instance left. Pure.
	 */
	inline ReloadProgress ProgressOf(const ReloadCounters& before, const ReloadCounters& now) {
		ReloadProgress progress;
		// No instance left (they were all stopped), or the counts started over: nothing more will come
		if (!now.present || now.requests < before.requests || now.acted < before.acted) {
			progress.done = true;
			return progress;
		}
		progress.instances = now.acted - before.acted;
		progress.playersMoved = now.playersMoved >= before.playersMoved ? now.playersMoved - before.playersMoved : 0;
		progress.failed = now.failed >= before.failed ? now.failed - before.failed : 0;
		progress.done = now.requests > before.requests && now.pending == 0;
		return progress;
	}
}
