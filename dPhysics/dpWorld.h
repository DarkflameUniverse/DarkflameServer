#pragma once

#include "Singleton.h"

//Navmesh includes:
#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"

#include <vector>
#include <map>

#include "dNavMesh.h"

class NiPoint3;
class dpEntity;
class dpGrid;

class dpWorld : public Singleton<dpWorld> {
public:
	void Initialize(unsigned int zoneID);

	~dpWorld();

	bool ShouldUseSP(unsigned int zoneID);
	bool IsLoaded() const { return m_NavMesh->GetdtNavMesh() != nullptr; }

	void StepWorld(float deltaTime);

	void AddEntity(dpEntity* entity);
	void RemoveEntity(dpEntity* entity);

	dNavMesh* GetNavMesh() { return m_NavMesh; }

private:
	dpGrid* m_Grid;
	bool phys_spatial_partitioning = 1;
	int phys_sp_tilesize = 205;
	int phys_sp_tilecount = 12;

	std::vector<dpEntity*> m_StaticEntities;
	std::vector<dpEntity*> m_DynamicEntites;

	dNavMesh* m_NavMesh = nullptr;
};
