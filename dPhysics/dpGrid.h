#pragma once
#include <vector>
#include <list>
#include <forward_list>
#include <map>
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

private:
	void HandleEntity(dpEntity* entity, dpEntity* other);
	void HandleCell(int x, int z, float deltaTime);

private:
	//cells on X, cells on Y for that X, then another vector that contains the entities within that cell.
	std::vector<std::vector<std::forward_list<dpEntity*>>> m_Cells;
	std::map<LWOOBJID, dpEntity*> m_GargantuanObjects;
};
