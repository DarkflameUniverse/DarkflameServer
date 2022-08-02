#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>

#include "DetourExtensions.h"

class NiPoint3;

class dNavMesh {
public:
	dNavMesh(uint32_t zoneId);
	~dNavMesh();

	float GetHeightAtPoint(const NiPoint3& location);
	std::vector<NiPoint3> GetPath(const NiPoint3& startPos, const NiPoint3& endPos, float speed = 10.0f);
private:
	void LoadNavmesh();

	uint32_t m_ZoneId;

	uint8_t* m_Triareas;
	rcHeightfield* m_Solid;
	rcCompactHeightfield* m_CHF;
	rcContourSet* m_CSet;
	rcPolyMesh* m_PMesh;
	rcConfig m_Config;
	rcPolyMeshDetail* m_PMDMesh;

	class InputGeom* m_Geometry;
	class dtNavMesh* m_NavMesh;
	class dtNavMeshQuery* m_NavQuery;
	uint8_t m_NavMeshDrawFlags;
	rcContext* m_Ctx;
};
