#pragma once

#include <cstdint>
#include <vector>
#include <fstream>

class RawHeightMap {
public:
	RawHeightMap();
	RawHeightMap(std::ifstream& stream, float height, float width);
	~RawHeightMap();

	uint32_t m_Unknown1;
	uint32_t m_Unknown2;
	uint32_t m_Unknown3;
	uint32_t m_Unknown4;

	float m_ScaleFactor;

	std::vector<float> m_FloatMap = {};
};
