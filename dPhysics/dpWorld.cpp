#include "dpWorld.h"
#include "dpEntity.h"
#include "dpGrid.h"
#include "DetourCommon.h"

#include <string>

#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

#include "dNavMesh.h"

namespace {
	dpGrid* m_Grid = nullptr;
	dNavMesh* m_NavMesh = nullptr;
	int32_t phys_sp_tilesize = 205;
	int32_t phys_sp_tilecount = 12;

	uint32_t m_ZoneID = 0;

	std::vector<dpEntity*> m_StaticEntities;
	std::vector<dpEntity*> m_DynamicEntites;
	bool phys_spatial_partitioning = true;
};

void dpWorld::Initialize(unsigned int zoneID, bool generateNewNavMesh) {
	const auto physSpTilecount = Game::config->GetValue("phys_sp_tilecount");
	if (!physSpTilecount.empty()) GeneralUtils::TryParse(physSpTilecount, phys_sp_tilecount);
	const auto physSpTilesize = Game::config->GetValue("phys_sp_tilesize");
	if (!physSpTilesize.empty()) GeneralUtils::TryParse(physSpTilesize, phys_sp_tilesize);
	const auto physSpatialPartitioning = Game::config->GetValue("phys_spatial_partitioning");
	if (!physSpatialPartitioning.empty()) phys_spatial_partitioning = physSpatialPartitioning == "1";

	//If spatial partitioning is enabled, then we need to create the m_Grid.
	//if m_Grid exists, then the old method will be used.
	//SP will NOT be used unless it is added to ShouldUseSP();
	if (ShouldUseSP(zoneID)) {
		m_Grid = new dpGrid(phys_sp_tilecount, phys_sp_tilesize);
	}

	if (generateNewNavMesh) m_NavMesh = new dNavMesh(zoneID);

	LOG("Physics world initialized!");
	m_ZoneID = zoneID;
}

void dpWorld::Reload() {
	if (m_Grid) {
		m_Grid->SetDeleteGrid(false);
		auto oldGridCells = m_Grid->GetCells();
		delete m_Grid;
		m_Grid = nullptr;

		Initialize(m_ZoneID, false);
		for (auto column : oldGridCells) {
			for (auto row : column) {
				for (auto entity : row) {
					AddEntity(entity);
				}
			}
		}
		LOG("Successfully reloaded physics world!");
	} else {
		LOG("No physics world to reload!");
	}
}

void dpWorld::Shutdown() {
	if (m_Grid) {
		// Triple check this is true
		m_Grid->SetDeleteGrid(true);
		delete m_Grid;
		m_Grid = nullptr;
	}

	if (m_NavMesh) {
		delete m_NavMesh;
		m_NavMesh = nullptr;
	}
}

bool dpWorld::IsLoaded() {
	return m_NavMesh->GetdtNavMesh() != nullptr;
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

dNavMesh* dpWorld::GetNavMesh() {
	return m_NavMesh;
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
	} else {
		if (entity->GetIsStatic()) {
			for (size_t i = 0; i < m_StaticEntities.size(); ++i) {
				if (m_StaticEntities[i] == entity) {
					delete m_StaticEntities[i];
					m_StaticEntities[i] = nullptr;
					break;
				}
			}
		} else {
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

bool dpWorld::ShouldUseSP(uint32_t zoneID) {
	if (!phys_spatial_partitioning) return false;

	// TODO: Add to this list as needed.
	// Only large maps should be added as tiling likely makes little difference on small maps.

	switch (zoneID) {
	case 1100: // Avant Gardens
	case 1200: // Nimbus Station
	case 1300: // Gnarled Forest
	case 1400: // Forbidden Valley
	case 1800: // Crux Prime
	case 1900: // Nexus Tower
	case 2000: // Ninjago
		return true;
	}

	return false;
}
