#pragma once
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include <array>
#include <string>
#include <vector>

// One of an object's render techniques (SceneLoader::ReadLvlObjectData, version 7 on)
struct LevelRenderTechnique {
	std::string name; // 64 bytes in the file
	uint32_t valueCount{};
	uint8_t isColor{};
	std::array<float, 16> values{};
};

struct SceneObject {
	LWOOBJID id{};
	LOT lot{};
	uint32_t nodeType = 1; // the client's default when the file has none, or one outside 0-10
	uint32_t glomId = 1;
	NiPoint3 position;
	NiQuaternion rotation = QuatUtils::IDENTITY;
	float scale = 1.0f;
	std::string renderTechniquesName; // 64 bytes in the file, only there when there are render techniques
	std::vector<LevelRenderTechnique> renderTechniques;
	LwoNameValue settings;
};

#define LOT_MARKER_PLAYER_START 1931
#define LOT_MARKET_CAMERA_TARGET 2182
