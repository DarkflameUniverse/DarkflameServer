#pragma once

#include "json.hpp"

struct WorldFilesStatus;

/**
 * World hot reload on the dashboard (docs/WorldHotReload.md): the zone files each running world loaded, whether they
 * changed on disk, and a Reload action per zone. The status comes from master (WORLD_FILES_STATUS) and is pushed to
 * browsers on the world_files socket topic.
 */
namespace WorldReloadRoutes {
	void RegisterRoutes();

	// Master sent a status
	void HandleStatus(const WorldFilesStatus& status);

	nlohmann::json StatusJson();
}
