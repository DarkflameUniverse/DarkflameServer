#pragma once

#include <vector>

#include "NiPoint3.h"

struct RawMesh {
	std::vector<NiPoint3> m_Vertices;
	std::vector<uint32_t> m_Triangles;
};
