#include "dpGrid.h"
#include "dpEntity.h"

#include <cmath>

dpGrid::dpGrid(int numCells, int cellSize) {
	NUM_CELLS = numCells;
	CELL_SIZE = cellSize;
	m_DeleteGrid = true;

	//dumb method but i can't be bothered

	//fill x
	for (int i = 0; i < NUM_CELLS; i++) {
		m_Cells.push_back(std::vector<std::forward_list<dpEntity*>>());
	}

	//fill z
	for (int i = 0; i < NUM_CELLS; i++) {
		for (int i = 0; i < NUM_CELLS; i++) {
			m_Cells[i].push_back(std::forward_list<dpEntity*>());
		}
	}
}

dpGrid::~dpGrid() {
	if (!this->m_DeleteGrid) return;
	for (auto& x : m_Cells) { //x
		for (auto& y : x) { //y
			for (auto en : y) {
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

	if (cellX < 0) cellX = 0;
	if (cellZ < 0) cellZ = 0;
	if (cellX > NUM_CELLS) cellX = NUM_CELLS;
	if (cellZ > NUM_CELLS) cellZ = NUM_CELLS;

	//Add to cell:
	m_Cells[cellX][cellZ].push_front(entity);

	//To verify that the object isn't gargantuan:
	if (entity->GetScale() >= CELL_SIZE * 2 || entity->GetIsGargantuan())
		m_GargantuanObjects.insert(std::make_pair(entity->m_ObjectID, entity));
}

void dpGrid::Move(dpEntity* entity, float x, float z) {
	int oldCellX = (int)std::round(entity->m_Position.x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = (int)std::round(entity->m_Position.z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	int cellX = (int)std::round(x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int cellZ = (int)std::round(z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	if (cellX < 0) cellX = 0;
	if (cellZ < 0) cellZ = 0;
	if (cellX > NUM_CELLS) cellX = NUM_CELLS;
	if (cellZ > NUM_CELLS) cellZ = NUM_CELLS;

	if (oldCellX < 0) oldCellX = 0;
	if (oldCellZ < 0) oldCellZ = 0;
	if (oldCellX > NUM_CELLS) oldCellX = NUM_CELLS;
	if (oldCellZ > NUM_CELLS) oldCellZ = NUM_CELLS;

	if (oldCellX == cellX && oldCellZ == cellZ) return;

	//Remove from perv cell:
	m_Cells[oldCellX][oldCellZ].remove(entity);

	//Add to the new cell
	m_Cells[cellX][cellZ].push_front(entity);
}

void dpGrid::Delete(dpEntity* entity) {
	if (!entity) return;
	int oldCellX = (int)std::round(entity->m_Position.x) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = (int)std::round(entity->m_Position.z) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	if (oldCellX < 0) oldCellX = 0;
	if (oldCellZ < 0) oldCellZ = 0;
	if (oldCellX > NUM_CELLS) oldCellX = NUM_CELLS;
	if (oldCellZ > NUM_CELLS) oldCellZ = NUM_CELLS;

	m_Cells[oldCellX][oldCellZ].remove(entity);

	if (m_GargantuanObjects.find(entity->m_ObjectID) != m_GargantuanObjects.end())
		m_GargantuanObjects.erase(entity->m_ObjectID);

	if (entity) delete entity;
	entity = nullptr;
}

void dpGrid::Update(float deltaTime) {
	//Pre-update:
	for (auto& x : m_Cells) { //x
		for (auto& y : x) { //y
			for (auto en : y) {
				if (!en) continue;
				en->PreUpdate();
			}
		}
	}

	//Actual collision detection update:
	for (int x = 0; x < NUM_CELLS; x++) {
		for (int y = 0; y < NUM_CELLS; y++) {
			HandleCell(x, y, deltaTime);
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

		for (auto other : m_GargantuanObjects)
			HandleEntity(en, other.second);
	}
}
