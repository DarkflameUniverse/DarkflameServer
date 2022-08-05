#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <cstring>

#include "DetourExtensions.h"

class NiPoint3;

class dNavMesh {
public:
	dNavMesh(uint32_t zoneId);
	~dNavMesh();

	float GetHeightAtPoint(const NiPoint3& location);
	std::vector<NiPoint3> GetPath(const NiPoint3& startPos, const NiPoint3& endPos, float speed = 10.0f);

	class dtNavMesh* GetdtNavMesh() { return m_NavMesh; }

private:
	void LoadNavmesh();

	uint32_t m_ZoneId;

	uint8_t* m_Triareas = nullptr;
	rcHeightfield* m_Solid = nullptr;
	rcCompactHeightfield* m_CHF = nullptr;
	rcContourSet* m_CSet = nullptr;
	rcPolyMesh* m_PMesh = nullptr;
	rcConfig m_Config;
	rcPolyMeshDetail* m_PMDMesh = nullptr;

	class InputGeom* m_Geometry = nullptr;
	class dtNavMesh* m_NavMesh = nullptr;
	class dtNavMeshQuery* m_NavQuery = nullptr;
	uint8_t m_NavMeshDrawFlags;
	rcContext* m_Ctx = nullptr;
};
