#pragma once

#include <cstdint>
#include <vector>

class NiPoint3;
class rcHeightfield;
class rcCompactHeightfield;
class rcContourSet;
class rcPolyMesh;
class rcPolyMeshDetail;
class InputGeom;
class dtNavMesh;
class dtNavMeshQuery;
class rcContext;

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

	bool IsNavmeshLoaded() { return m_NavMesh != nullptr; }

private:
	void LoadNavmesh();

	uint32_t m_ZoneId;

	dtNavMesh* m_NavMesh = nullptr;
	dtNavMeshQuery* m_NavQuery = nullptr;
	uint8_t m_NavMeshDrawFlags;
};
