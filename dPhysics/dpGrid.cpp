#include "dpGrid.h"
#include "dpEntity.h"

#include <cmath>

dpGrid::dpGrid(int numCells, int cellSize) {
	NUM_CELLS = numCells;
	CELL_SIZE = cellSize;
	m_DeleteGrid = true;

	m_Cells.resize(NUM_CELLS, std::vector<std::vector<dpEntity*>>(NUM_CELLS));
}

dpGrid::~dpGrid() {
	if (!this->m_DeleteGrid) return;
	for (auto& x : m_Cells) { //x
		for (auto& z : x) { //y
			for (auto en : z) {
				if (!en) continue;
				delete en;
				en = nullptr;
			}
		}
	}
}

void dpGrid::Add(dpEntity* entity) {
	//Determine which grid cell it's in.
	int cellX = (int)std::round(entity->m_Position.x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int cellZ = (int)std::round(entity->m_Position.z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	// Clamp values to the range [0, NUM_CELLS - 1]
	cellX = std::clamp(cellX, 0, NUM_CELLS - 1);
	cellZ = std::clamp(cellZ, 0, NUM_CELLS - 1);

	//Add to cell:
	m_Cells[cellX][cellZ].push_back(entity);

	//To verify that the object isn't gargantuan:
	if (entity->GetScale() >= CELL_SIZE * 2 || entity->GetIsGargantuan())
		m_GargantuanObjects.insert(std::make_pair(entity->m_ObjectID, entity));
}

void dpGrid::Move(dpEntity* entity, float x, float z) {
	int oldCellX = (int)std::round(entity->m_Position.x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = (int)std::round(entity->m_Position.z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	int cellX = (int)std::round(x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int cellZ = (int)std::round(z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	// Clamp values to the range [0, NUM_CELLS - 1]
	cellX = std::clamp(cellX, 0, NUM_CELLS - 1);
	cellZ = std::clamp(cellZ, 0, NUM_CELLS - 1);

	oldCellX = std::clamp(oldCellX, 0, NUM_CELLS - 1);
	oldCellZ = std::clamp(oldCellZ, 0, NUM_CELLS - 1);

	if (oldCellX == cellX && oldCellZ == cellZ) return;

	//Remove from prev cell:
	auto& cell = m_Cells[oldCellX][oldCellZ];
	
	// For speed, find the single match and swap it with the last element, then pop_back.
	auto toRemove = std::find(cell.begin(), cell.end(), entity);
	if (toRemove != cell.end()) {
		*toRemove = cell.back();
		cell.pop_back();
	}

	//Add to the new cell
	m_Cells[cellX][cellZ].push_back(entity);
}

void dpGrid::Delete(dpEntity* entity) {
	if (!entity) return;
	int oldCellX = (int)std::round(entity->m_Position.x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = (int)std::round(entity->m_Position.z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	// Clamp values to the range [0, NUM_CELLS - 1]
	oldCellX = std::clamp(oldCellX, 0, NUM_CELLS - 1);
	oldCellZ = std::clamp(oldCellZ, 0, NUM_CELLS - 1);

	auto& cell = m_Cells[oldCellX][oldCellZ];
	auto toRemove = std::find(cell.begin(), cell.end(), entity);
	if (toRemove != cell.end()) {
		*toRemove = cell.back();
		cell.pop_back();
	}

	m_GargantuanObjects.erase(entity->m_ObjectID);

	if (entity) delete entity;
	entity = nullptr;
}

void dpGrid::Update(float deltaTime) {
	//Pre-update:
	for (auto& x : m_Cells) { //x
		for (auto& z : x) { //y
			for (auto en : z) {
				if (!en) continue;
				en->PreUpdate();
			}
		}
	}

	//Actual collision detection update:
	for (int x = 0; x < NUM_CELLS; x++) {
		for (int z = 0; z < NUM_CELLS; z++) {
			HandleCell(x, z, deltaTime);
		}
	}
}

void dpGrid::HandleEntity(dpEntity* entity, dpEntity* other) {
	if (!entity || !other) return;

	if (other->GetIsStatic())
		other->CheckCollision(entity); //swap "other" and "entity" if you want dyn objs to handle collisions.
}

void dpGrid::HandleCell(int x, int z, float deltaTime) {
	auto& entities = m_Cells[x][z]; //vector of entities contained within this cell.

	for (auto en : entities) {
		if (!en) continue;
		if (en->GetIsStatic() || en->GetSleeping()) continue;

		//Check against all entities that are in the same cell as us
		for (auto other : entities)
			HandleEntity(en, other);

		//To try neighbouring cells as well: (can be disabled if needed)
		//we only check 4 of the 8 neighbouring cells, otherwise we'd get duplicates and cpu cycles wasted...

		if (x > 0 && z > 0) {
			for (auto other : m_Cells[x - 1][z - 1])
				HandleEntity(en, other);
		}

		if (x > 0) {
			for (auto other : m_Cells[x - 1][z])
				HandleEntity(en, other);
		}

		if (z > 0) {
			for (auto other : m_Cells[x][z - 1])
				HandleEntity(en, other);
		}

		if (x > 0 && z < NUM_CELLS - 1) {
			for (auto other : m_Cells[x - 1][z + 1])
				HandleEntity(en, other);
		}

		for (auto& [id, entity] : m_GargantuanObjects)
			HandleEntity(en, entity);
	}
}
