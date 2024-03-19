#pragma once
#include <map>
#include <vector>

#include "dCommonVars.h"

class dpEntity;

class dpGrid {
public:
	//LU has a chunk size of 64x64, with each chunk unit being 3.2 ingame units.
	int NUM_CELLS = 12; //Most worlds consist of 10 or 11 chunks, so I'm picking 12 to be safe.
	int CELL_SIZE = 205; //64 * 3.2 = 204.8 rounded up

public:
	dpGrid(int numCells, int cellSize);
	~dpGrid();

	void Add(dpEntity* entity);
	void Move(dpEntity* entity, float x, float z);
	void Delete(dpEntity* entity);

	void Update(float deltaTime);

	/**
	 * Sets the delete grid parameter to value.  When false, the grid will not clean up memory.
	 *
	 * @param value Whether or not to delete entities on deletion of the grid.
	 */
	void SetDeleteGrid(bool value) { this->m_DeleteGrid = value; };

	// Intentional copy since this is only used when we delete this class to re-create it.
	std::vector<std::vector<std::vector<dpEntity*>>> GetCells() { return this->m_Cells; };

private:
	void HandleEntity(dpEntity* entity, dpEntity* other);
	void HandleCell(int x, int z, float deltaTime);

private:
	//cells on X, cells on Y for that X, then another vector that contains the entities within that cell.
	std::vector<std::vector<std::vector<dpEntity*>>> m_Cells;
	std::map<LWOOBJID, dpEntity*> m_GargantuanObjects;
	bool m_DeleteGrid = true;
};
