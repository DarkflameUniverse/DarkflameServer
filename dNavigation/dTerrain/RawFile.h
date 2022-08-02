#pragma once

#include <string>
#include <vector>

#include "BinaryIO.h"

#include "RawChunk.h"

class RawFile {
public:
	RawFile(std::string filePath);
	~RawFile();

private:

	void GenerateFinalMeshFromChunks();
	void WriteFinalMeshToOBJ(std::string path);

	uint8_t m_Version;
	uint16_t m_Padding;
	uint32_t m_ChunkCount;
	uint32_t m_Width;
	uint32_t m_Height;

	std::vector<RawChunk> m_Chunks;
	RawMesh m_FinalMesh;
};
