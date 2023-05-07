#pragma once

#include <vector>

#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"

class Spawner;

struct EntityInfo {
	EntityInfo() {
		spawner = nullptr;
		spawnerID = 0;
		hasSpawnerNodeID = false;
		spawnerNodeID = 0;
		id = 0;
		lot = LOT_NULL;
		pos = { 0,0,0 };
		rot = { 0,0,0,0 };
		settings = {};
		networkSettings = {};
		scale = 1.0f;
	}

	Spawner* spawner;
	LWOOBJID spawnerID;

	bool hasSpawnerNodeID;
	uint32_t spawnerNodeID;

	LWOOBJID id;
	LOT lot;
	NiPoint3 pos;
	NiQuaternion rot;
	std::vector<LDFBaseData*> settings;
	std::vector<LDFBaseData*> networkSettings;
	float scale;
};
