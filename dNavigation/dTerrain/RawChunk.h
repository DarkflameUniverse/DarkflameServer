#pragma once

#include <cstdint>
#include <fstream>

struct RawMesh;
class RawHeightMap;

class RawChunk {
public:
	RawChunk(std::ifstream& stream);
	~RawChunk();

	void GenerateMesh();

	uint32_t m_ChunkIndex;
	uint32_t m_Width;
	uint32_t m_Height;
	float m_X;
	float m_Z;

	RawHeightMap* m_HeightMap;
	RawMesh* m_Mesh;
};
