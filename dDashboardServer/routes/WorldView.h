#pragma once

struct PlayerPositions;

/**
 * The live 3D world view (/world3d): a zone's terrain, scene objects, paths and spawn points with players moving on
 * it, plus replays of where players went (position samples kept for position_history_days, staff only) and a
 * timelapse of the economy map events per day.
 */
namespace WorldView {
	void RegisterRoutes();

	// Read the CDClient data the zone data builders need (at startup: they run on worker threads)
	void Preload();

	// A world server reported its players' positions: keep some of them for replays (throttled, written in batches)
	void RecordPositions(const PlayerPositions& positions);
}
