#pragma once

#include <cstdint>

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
};
