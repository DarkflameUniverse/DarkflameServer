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

	m_NavMesh = new dNavMesh(zoneID);

	Game::logger->Log("dpWorld", "Physics world initialized!");
}

dpWorld::~dpWorld() {
	if (m_Grid) {
		delete m_Grid;
		m_Grid = nullptr;
	}

	if (m_NavMesh) {
		delete m_NavMesh;
		m_NavMesh = nullptr;
	}
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

bool dpWorld::ShouldUseSP(unsigned int zoneID) {
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
