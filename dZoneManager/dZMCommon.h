#pragma once
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include <vector>

struct mapCompareLwoSceneIDs {
	bool operator()(const LWOSCENEID& lhs, const LWOSCENEID& rhs) const { return lhs < rhs; }
};

struct SceneObject {
	LWOOBJID id;
	LOT lot;
	uint32_t value1;
	uint32_t value2;
	NiPoint3 position;
	NiQuaternion rotation;
	float scale = 1.0f;
	//std::string settings;
	uint32_t value3;
	std::vector<LDFBaseData*> settings;
};

#define LOT_MARKER_PLAYER_START 1931
#define LOT_MARKET_CAMERA_TARGET 2182
