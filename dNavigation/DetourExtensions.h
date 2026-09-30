#pragma once

#include <cstdint>

#include "Recast.h"
#include "DetourCommon.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"

static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; // char[4] of 'MSET'
static const int NAVMESHSET_VERSION = 1;

// Read straight from the .bin navmesh files
struct NavMeshSetHeader {
	int32_t magic;
	int32_t version;
	int32_t numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader {
	dtTileRef tileRef;
	int32_t dataSize;
};

// The layout the files were always read with on 64-bit builds
static_assert(sizeof(NavMeshSetHeader) == 12 + sizeof(dtNavMeshParams));
static_assert(sizeof(NavMeshTileHeader) == (sizeof(dtTileRef) == 8 ? 16 : 8));

static const int MAX_POLYS = 256;
static const int MAX_SMOOTH = 2048;
