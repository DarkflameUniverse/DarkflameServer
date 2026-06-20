#pragma once
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include <vector>

struct SceneObject {
	LWOOBJID id;
	LOT lot;
	uint32_t nodeType;
	uint32_t glomId;
	NiPoint3 position;
	NiQuaternion rotation = QuatUtils::IDENTITY;
	float scale = 1.0f;
	uint32_t value3;
	LwoNameValue settings;
};

#define LOT_MARKER_PLAYER_START 1931
#define LOT_MARKET_CAMERA_TARGET 2182
