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

	dNavMesh* GetNavMesh();
};
