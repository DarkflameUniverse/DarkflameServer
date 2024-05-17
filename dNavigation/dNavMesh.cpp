#include "dNavMesh.h"

#include "RawFile.h"

#include "Game.h"
#include "Logger.h"
#include "dPlatforms.h"
#include "NiPoint3.h"
#include "BinaryIO.h"
#include "BinaryPathFinder.h"

#include "dZoneManager.h"
#include "DluAssert.h"
#include "DetourExtensions.h"

dNavMesh::dNavMesh(uint32_t zoneId) {
	m_ZoneId = zoneId;

	this->LoadNavmesh();

	if (m_NavMesh) {
		m_NavQuery = dtAllocNavMeshQuery();
		m_NavQuery->init(m_NavMesh, 2048);

		LOG("Navmesh loaded successfully!");
	} else {
		LOG("Navmesh loading failed (This may be intended).");
	}
}

dNavMesh::~dNavMesh() {
	// Clean up Recast information

	if (m_NavMesh) dtFreeNavMesh(m_NavMesh);
	if (m_NavQuery) dtFreeNavMeshQuery(m_NavQuery);
}


void dNavMesh::LoadNavmesh() {

	std::string path = (BinaryPathFinder::GetBinaryDir() / "navmeshes/" / (std::to_string(m_ZoneId) + ".bin")).string();

	if (!BinaryIO::DoesFileExist(path)) {
		return;
	}

	FILE* fp;

#ifdef _WIN32
	fopen_s(&fp, path.c_str(), "rb");
#elif __APPLE__
	// macOS has 64bit file IO by default
	fp = fopen(path.c_str(), "rb");
#else
	fp = fopen64(path.c_str(), "rb");
#endif

	if (!fp) {
		return;
	}

	// Read header.
	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1) {
		fclose(fp);
		return;
	}

	if (header.magic != NAVMESHSET_MAGIC) {
		fclose(fp);
		return;
	}

	if (header.version != NAVMESHSET_VERSION) {
		fclose(fp);
		return;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh) {
		fclose(fp);
		return;
	}

	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status)) {
		fclose(fp);
		return;
	}

	// Read tiles.
	for (int i = 0; i < header.numTiles; ++i) {
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1) return;

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = static_cast<unsigned char*>(dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM));
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1) return;

		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
	}

	fclose(fp);

	m_NavMesh = mesh;
}

NiPoint3 dNavMesh::NearestPoint(const NiPoint3& location, const float halfExtent) const {
	NiPoint3 toReturn = location;
	if (m_NavMesh != nullptr) {
		float pos[3];
		pos[0] = location.x;
		pos[1] = location.y;
		pos[2] = location.z;

		dtPolyRef nearestRef = 0;
		float polyPickExt[3] = { halfExtent, halfExtent, halfExtent };
		float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };
		dtQueryFilter filter{};

		auto hasPoly = m_NavQuery->findNearestPoly(pos, polyPickExt, &filter, &nearestRef, nearestPoint);
		if (hasPoly != DT_SUCCESS) {
			toReturn = location;
		} else {
			toReturn.x = nearestPoint[0];
			toReturn.y = nearestPoint[1];
			toReturn.z = nearestPoint[2];
		}
	}
	return toReturn;
}

float dNavMesh::GetHeightAtPoint(const NiPoint3& location, const float halfExtentsHeight) const {
	if (m_NavMesh == nullptr) {
		return location.y;
	}

	float toReturn = 0.0f;
	float pos[3];
	pos[0] = location.x;
	pos[1] = location.y;
	pos[2] = location.z;

	dtPolyRef nearestRef = 0;
	float polyPickExt[3] = { 32.0f, halfExtentsHeight, 32.0f };
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };
	dtQueryFilter filter{};

	auto hasPoly = m_NavQuery->findNearestPoly(pos, polyPickExt, &filter, &nearestRef, nearestPoint);
	m_NavQuery->getPolyHeight(nearestRef, pos, &toReturn);
#ifdef _DEBUG
	if (toReturn != 0.0f) {
		DluAssert(toReturn == nearestPoint[1]);
	}
#endif
	if (toReturn == 0.0f) {
		// If we were unable to get the poly height, but the query returned a success, just use the height of the nearest point.
		// This is what seems to happen anyways and it is better than returning zero.
		if (hasPoly == DT_SUCCESS) {
			toReturn = nearestPoint[1];
		} else {
			toReturn = location.y;
		}
	}
	// If we failed to even find a poly, do not change the height since we have no idea what it should be.

	return toReturn;
}

std::vector<NiPoint3> dNavMesh::GetPath(const NiPoint3& startPos, const NiPoint3& endPos, float speed) {
	std::vector<NiPoint3> path;

	// Allows for non-navmesh maps (like new custom maps) to have "basic" enemies.
	if (m_NavMesh == nullptr) {
		// How many points to generate between start/end?
		// Note: not actually 100% accurate due to rounding, but worst case it causes them to go a tiny bit faster
		// than their speed value would normally allow at the end.
		int numPoints = startPos.Distance(startPos, endPos) / speed;

		path.push_back(startPos); //insert the start pos

		// Linearly interpolate between these two points:
		for (int i = 0; i < numPoints; i++) {
			NiPoint3 newPoint{ startPos };

			newPoint.x += speed;
			newPoint.y = newPoint.y + (((endPos.y - startPos.y) / (endPos.x - startPos.x)) * (newPoint.x - startPos.x));

			path.push_back(newPoint);
		}

		path.push_back(endPos); //finally insert our end pos

		return path;
	}

	float sPos[3];
	float ePos[3];
	sPos[0] = startPos.x;
	sPos[1] = startPos.y;
	sPos[2] = startPos.z;

	ePos[0] = endPos.x;
	ePos[1] = endPos.y;
	ePos[2] = endPos.z;

	dtStatus pathFindStatus;
	dtPolyRef startRef;
	dtPolyRef endRef;
	float polyPickExt[3] = { 32.0f, 32.0f, 32.0f };
	dtQueryFilter filter{};

	//Find our start poly
	m_NavQuery->findNearestPoly(sPos, polyPickExt, &filter, &startRef, 0);

	//Find our end poly
	m_NavQuery->findNearestPoly(ePos, polyPickExt, &filter, &endRef, 0);

	pathFindStatus = DT_FAILURE;
	int m_nstraightPath = 0;
	int m_npolys = 0;
	dtPolyRef m_polys[MAX_POLYS];
	float m_straightPath[MAX_POLYS * 3];
	unsigned char m_straightPathFlags[MAX_POLYS];
	dtPolyRef m_straightPathPolys[MAX_POLYS];
	int m_straightPathOptions = 0;

	if (startRef && endRef) {
		m_NavQuery->findPath(startRef, endRef, sPos, ePos, &filter, m_polys, &m_npolys, MAX_POLYS);

		if (m_npolys) {
			// In case of partial path, make sure the end point is clamped to the last polygon.
			float epos[3];
			dtVcopy(epos, ePos);

			if (m_polys[m_npolys - 1] != endRef) {
				m_NavQuery->closestPointOnPoly(m_polys[m_npolys - 1], ePos, epos, 0);
			}

			m_NavQuery->findStraightPath(sPos, epos, m_polys, m_npolys,
				m_straightPath, m_straightPathFlags,
				m_straightPathPolys, &m_nstraightPath, MAX_POLYS, m_straightPathOptions);

			// At this point we have our path. Copy it to the path store
			int nIndex = 0;
			for (int nVert = 0; nVert < m_nstraightPath; nVert++) {
				NiPoint3 newPoint{ m_straightPath[nIndex++], m_straightPath[nIndex++], m_straightPath[nIndex++] };
				path.push_back(newPoint);
			}
		}
	} else {
		m_npolys = 0;
		m_nstraightPath = 0;
	}

	return path;
}
