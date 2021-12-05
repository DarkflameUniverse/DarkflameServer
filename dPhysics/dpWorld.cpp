#include "dpWorld.h"
#include "dpEntity.h"
#include "dpGrid.h"
#include "DetourCommon.h"

#include <string>

#include "Game.h"
#include "dLogger.h"
#include "dConfig.h"

void dpWorld::Initialize(unsigned int zoneID) {
	phys_sp_tilecount = std::atoi(Game::config->GetValue("phys_sp_tilecount").c_str());
	phys_sp_tilesize = std::atoi(Game::config->GetValue("phys_sp_tilesize").c_str());

	//If spatial partitioning is enabled, then we need to create the m_Grid. 
	//if m_Grid exists, then the old method will be used.
	//SP will NOT be used unless it is added to ShouldUseSP();
	if (std::atoi(Game::config->GetValue("phys_spatial_partitioning").c_str()) == 1
		&& ShouldUseSP(zoneID)) {
		m_Grid = new dpGrid(phys_sp_tilecount, phys_sp_tilesize);
	}

	Game::logger->Log("dpWorld", "Physics world initialized!\n");

	if (ShouldLoadNavmesh(zoneID)) {
		if (LoadNavmeshByZoneID(zoneID)) Game::logger->Log("dpWorld", "Loaded navmesh!\n");
		else Game::logger->Log("dpWorld", "Error(s) occurred during navmesh load.\n");
	}
}

dpWorld::~dpWorld() {
	if (m_Grid) {
		delete m_Grid;
		m_Grid = nullptr;
	}

	RecastCleanup();
}

void dpWorld::StepWorld(float deltaTime) {
	if (m_Grid) { 
		m_Grid->Update(deltaTime); 
		return; 
	}

	//Pre update:
	for (auto entity : m_StaticEntities) {
		if (!entity || entity->GetSleeping()) continue;
		entity->PreUpdate();
	}

	//Do actual update:
	for (auto entity : m_DynamicEntites) {
		if (!entity || entity->GetSleeping()) continue;
		
		entity->Update(deltaTime);

		for (auto other : m_StaticEntities) {
			if (!other || other->GetSleeping() || entity->GetObjectID() == other->GetObjectID()) continue;

			other->CheckCollision(entity); //swap "other" and "entity" if you want dyn objs to handle collisions.
		}
	}
}

void dpWorld::AddEntity(dpEntity* entity) {
	if (m_Grid) entity->SetGrid(m_Grid); //This sorts this entity into the right cell
	else { //old method, slow
		if (entity->GetIsStatic()) m_StaticEntities.push_back(entity);
		else m_DynamicEntites.push_back(entity);
	}
}

void dpWorld::RemoveEntity(dpEntity* entity) {
	if (!entity) return;

	if (m_Grid) {
		m_Grid->Delete(entity);
	}
	else {
		if (entity->GetIsStatic()) {
			for (size_t i = 0; i < m_StaticEntities.size(); ++i) {
				if (m_StaticEntities[i] == entity) {
					delete m_StaticEntities[i];
					m_StaticEntities[i] = nullptr;
					break;
				}
			}
		}
		else {
			for (size_t i = 0; i < m_DynamicEntites.size(); ++i) {
				if (m_DynamicEntites[i] == entity) {
					delete m_DynamicEntites[i];
					m_DynamicEntites[i] = nullptr;
					break;
				}
			}
		}
	}
}

void dpWorld::RecastCleanup() {
	if (m_triareas) delete[] m_triareas;
	m_triareas = 0;

	rcFreeHeightField(m_solid);
	m_solid = 0;
	rcFreeCompactHeightfield(m_chf);
	m_chf = 0;
	rcFreeContourSet(m_cset);
	m_cset = 0;
	rcFreePolyMesh(m_pmesh);
	m_pmesh = 0;
	rcFreePolyMeshDetail(m_dmesh);
	m_dmesh = 0;
	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;

	dtFreeNavMeshQuery(m_navQuery);
	m_navQuery = 0;

	if (m_ctx) delete m_ctx;
}

bool dpWorld::LoadNavmeshByZoneID(unsigned int zoneID) {
	std::string path = "./res/maps/navmeshes/" + std::to_string(zoneID) + ".bin";
	m_navMesh = LoadNavmesh(path.c_str());

	if (m_navMesh) { m_navQuery = dtAllocNavMeshQuery(); m_navQuery->init(m_navMesh, 2048); }
	else return false;

	return true;
}

dtNavMesh* dpWorld::LoadNavmesh(const char* path) {
	FILE* fp;
	
	#ifdef _WIN32
		fopen_s(&fp, path, "rb");
    #elif __APPLE__
		// macOS has 64bit file IO by default
		fp = fopen(path, "rb");
	#else
		fp = fopen64(path, "rb");
	#endif
	
	if (!fp) {
		return 0;
	}

	// Read header.
	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1) {
		fclose(fp);
		return 0;
	}

	if (header.magic != NAVMESHSET_MAGIC) {
		fclose(fp);
		return 0;
	}

	if (header.version != NAVMESHSET_VERSION) {
		fclose(fp);
		return 0;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh) {
		fclose(fp);
		return 0;
	}

	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status)) {
		fclose(fp);
		return 0;
	}

	// Read tiles.
	for (int i = 0; i < header.numTiles; ++i) {
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
			return 0;

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1)
			return 0;

		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
	}

	fclose(fp);

	return mesh;
}

bool dpWorld::ShouldLoadNavmesh(unsigned int zoneID) {
	return true; //We use default paths now. Might re-tool this function later.

	//TODO: Add to this list as the navmesh folder grows.
	switch (zoneID) {
	case 1100:
	case 1150:
	case 1151:
	case 1200:
	case 1201:
	case 1300:
	case 1400:
	case 1603:
		return true;
	}

	return false;
}

bool dpWorld::ShouldUseSP(unsigned int zoneID) {
	//TODO: Add to this list as needed. Only large maps should be added as tiling likely makes little difference on small maps.
	switch (zoneID) {
	case 1100: //Avant Gardens
	case 1200: //Nimbus Station
	case 1300: //Gnarled Forest
	case 1400: //Forbidden Valley
	case 1800: //Crux Prime
	case 1900: //Nexus Tower
	case 2000: //Ninjago
		return true;
	}

	return false;
}

float dpWorld::GetHeightAtPoint(const NiPoint3& location) {
	if (m_navMesh == nullptr) {
		return location.y;
	}

	float toReturn = 0.0f;
	float pos[3];
	pos[0] = location.x;
	pos[1] = location.y;
	pos[2] = location.z;

	dtPolyRef nearestRef = 0;
	float polyPickExt[3] = { 32.0f, 32.0f, 32.0f };
	dtQueryFilter filter{};

	m_navQuery->findNearestPoly(pos, polyPickExt, &filter, &nearestRef, 0);
	m_navQuery->getPolyHeight(nearestRef, pos, &toReturn);

	if (toReturn == 0.0f) {
		toReturn = location.y;
	}

	return toReturn;
}

std::vector<NiPoint3> dpWorld::GetPath(const NiPoint3& startPos, const NiPoint3& endPos, float speed) {
	std::vector<NiPoint3> path;

	//allows for non-navmesh maps (like new custom maps) to have "basic" enemies.
	if (m_navMesh == nullptr) {
		//how many points to generate between start/end?
		//note: not actually 100% accurate due to rounding, but worst case it causes them to go a tiny bit faster
		//than their speed value would normally allow at the end.
		int numPoints = startPos.Distance(startPos, endPos) / speed; 

		path.push_back(startPos); //insert the start pos

		//Linearly interpolate between these two points:
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
	m_navQuery->findNearestPoly(sPos, polyPickExt, &filter, &startRef, 0);

	//Find our end poly
	m_navQuery->findNearestPoly(ePos, polyPickExt, &filter, &endRef, 0);

	pathFindStatus = DT_FAILURE;
	int m_nstraightPath = 0;
	int m_npolys = 0;
	dtPolyRef m_polys[MAX_POLYS];
	float m_straightPath[MAX_POLYS * 3];
	unsigned char m_straightPathFlags[MAX_POLYS];
	dtPolyRef m_straightPathPolys[MAX_POLYS];
	int m_straightPathOptions = 0;

	if (startRef && endRef) {
		m_navQuery->findPath(startRef, endRef, sPos, ePos, &filter, m_polys, &m_npolys, MAX_POLYS);

		if (m_npolys) {
			// In case of partial path, make sure the end point is clamped to the last polygon.
			float epos[3];
			dtVcopy(epos, ePos);
			if (m_polys[m_npolys - 1] != endRef)
				m_navQuery->closestPointOnPoly(m_polys[m_npolys - 1], ePos, epos, 0);

			m_navQuery->findStraightPath(sPos, epos, m_polys, m_npolys,
				m_straightPath, m_straightPathFlags,
				m_straightPathPolys, &m_nstraightPath, MAX_POLYS, m_straightPathOptions);

			// At this point we have our path.  Copy it to the path store
			int nIndex = 0;
			for (int nVert = 0; nVert < m_nstraightPath; nVert++) {
				/*m_PathStore[nPathSlot].PosX[nVert] = StraightPath[nIndex++];
				m_PathStore[nPathSlot].PosY[nVert] = StraightPath[nIndex++];
				m_PathStore[nPathSlot].PosZ[nVert] = StraightPath[nIndex++];*/

				NiPoint3 newPoint{ m_straightPath[nIndex++], m_straightPath[nIndex++], m_straightPath[nIndex++] };
				path.push_back(newPoint);
			}
		}
	}
	else {
		m_npolys = 0;
		m_nstraightPath = 0;
	}

	return path;
}
