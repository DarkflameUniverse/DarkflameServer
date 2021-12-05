#pragma once
#include "Singleton.h"
#include <vector>

//Navmesh includes:
#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"

#include <vector>
#include <map>

static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; //'MSET';
static const int NAVMESHSET_VERSION = 1;

struct NavMeshSetHeader {
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader {
	dtTileRef tileRef;
	int dataSize;
};

static const int MAX_POLYS = 256;
static const int MAX_SMOOTH = 2048;

class NiPoint3;
class dpEntity;
class dpGrid;

class dpWorld : public Singleton<dpWorld> {
public:
	void Initialize(unsigned int zoneID);

	~dpWorld();
	void RecastCleanup();

	bool LoadNavmeshByZoneID(unsigned int zoneID);
	dtNavMesh* LoadNavmesh(const char* path);
	bool ShouldLoadNavmesh(unsigned int zoneID);
	bool ShouldUseSP(unsigned int zoneID);

	float GetHeightAtPoint(const NiPoint3& location);
	std::vector<NiPoint3> GetPath(const NiPoint3& startPos, const NiPoint3& endPos, float speed = 10.0f);
	bool IsLoaded() const { return m_navMesh != nullptr; }

	void StepWorld(float deltaTime);

	void AddEntity(dpEntity* entity);
	void RemoveEntity(dpEntity* entity);

private:
	dpGrid* m_Grid;
	bool phys_spatial_partitioning = 1;
	int phys_sp_tilesize = 205;
	int phys_sp_tilecount = 12;

	std::vector<dpEntity*> m_StaticEntities;
	std::vector<dpEntity*> m_DynamicEntites;

	//Navmesh stuffs:
	unsigned char* m_triareas;
	rcHeightfield* m_solid;
	rcCompactHeightfield* m_chf;
	rcContourSet* m_cset;
	rcPolyMesh* m_pmesh;
	rcConfig m_cfg;
	rcPolyMeshDetail* m_dmesh;

	class InputGeom* m_geom;
	class dtNavMesh* m_navMesh;
	class dtNavMeshQuery* m_navQuery;
	unsigned char m_navMeshDrawFlags;
	rcContext* m_ctx;
};