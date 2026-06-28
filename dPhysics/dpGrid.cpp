#include "dpGrid.h"
#include "dpEntity.h"

#include <cmath>
#include <ranges>

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
	int cellX = static_cast<int>(std::round(entity->m_Position.x)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int cellZ = static_cast<int>(std::round(entity->m_Position.z)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

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
	int oldCellX = static_cast<int>(std::round(entity->m_Position.x)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = static_cast<int>(std::round(entity->m_Position.z)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

	int cellX = static_cast<int>(std::round(x)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int cellZ = static_cast<int>(std::round(z)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

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
	int oldCellX = static_cast<int>(std::round(entity->m_Position.x)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;
	int oldCellZ = static_cast<int>(std::round(entity->m_Position.z)) / dpGrid::CELL_SIZE + NUM_CELLS / 2;

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

	for (auto* en : entities) {
		if (!en) continue;
		if (en->GetIsStatic() || en->GetSleeping()) continue;

		//Check against all entities that are in the same cell as us
		for (auto other : entities) HandleEntity(en, other);

		// All 8 neighbours in one pass.
		// staticOnly=false  — canonical 4: covers each dynamic-vs-dynamic pair exactly once,
		//                     since the higher-index cell checks back to the lower-index cell.
		// staticOnly=true   — skipped 4: dynamic entities there are handled when those cells
		//                     process their own en loop; static ones never drive a loop, so
		//                     we handle them here explicitly to avoid missing exits.
		struct NeighbourCheck { int dx, dz; bool staticOnly; };
		constexpr NeighbourCheck kNeighbours[8] = {
			{ -1, -1, false }, { -1,  0, false }, {  0, -1, false }, { -1,  1, false },
			{  1, -1, true  }, {  1,  0, true  }, {  0,  1, true  }, {  1,  1, true  },
		};
		for (auto [dx, dz, staticOnly] : kNeighbours) {
			const int nx = x + dx;
			const int nz = z + dz;
			// Ensure the cell we're checking is within the valid range
			if (nx < 0 || nx >= NUM_CELLS || nz < 0 || nz >= NUM_CELLS) continue;
			for (auto* other : m_Cells[nx][nz]) {
				if (!staticOnly || (other && other->GetIsStatic()))
					HandleEntity(en, other);
			}
		}

		for (auto* entity : m_GargantuanObjects | std::views::values) HandleEntity(en, entity);
	}
}
