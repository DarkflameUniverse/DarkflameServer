#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "dpMovementBlockers.h"

class dNavMesh;
class dpEntity;

namespace dpWorld {
	void Initialize(uint32_t zoneID, bool generateNewNavMesh = true);
	void Shutdown();
	void Reload();

	bool ShouldUseSP(uint32_t zoneID);
	bool IsLoaded();

	void StepWorld(float deltaTime);

	void AddEntity(dpEntity* entity);
	void RemoveEntity(dpEntity* entity);

	/**
	 * Takes an entity out of the world without deleting it, so it can be added back later (a volume switched off)
	 */
	void DetachEntity(dpEntity* entity);

	dNavMesh* GetNavMesh();

	/**
	 * A wall the server's movers (MovementAI) cannot walk through (see dpMovementBlocker). The entity stays owned by
	 * the caller, who removes it before deleting it; it is not stepped with the world.
	 */
	void AddMovementBlocker(const dpEntity* entity, uint32_t filter);
	/**
	 * A movement blocker with no object of its own (a wall from the level files the server doesn't spawn), owned by the
	 * world from now on and freed when it shuts down.
	 */
	void AddOwnedMovementBlocker(dpEntity* entity, uint32_t filter);
	void RemoveMovementBlocker(const dpEntity* entity);
	std::span<const dpMovementBlocker> GetMovementBlockers();

	// A mover's path cut where it first walks into a movement blocker (dpMovementBlockers::ClampPath)
	std::vector<NiPoint3> ClampPath(const NiPoint3& start, std::vector<NiPoint3> path, uint32_t moverFilter);
};
