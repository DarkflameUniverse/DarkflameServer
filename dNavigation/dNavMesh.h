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

	/**
	 * Get the height at a point
	 * 
	 * @param location The location to check for height at. This is the center of the search area.
	 * @param halfExtentsHeight The half extents height of the search area. This is the distance from the center to the top and bottom of the search area.
	 * The larger the value of halfExtentsHeight is, the larger the performance cost of the query.
	 * @return float The height at the point. If the point is not on the navmesh, the height of the point is returned.
	 */
	float GetHeightAtPoint(const NiPoint3& location, const float halfExtentsHeight = 32.0f) const;
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
