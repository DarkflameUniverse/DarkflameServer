#include "RawChunk.h"

#include "BinaryIO.h"

#include "RawMesh.h"
#include "RawHeightMap.h"

RawChunk::RawChunk(std::ifstream& stream) {
	// Read the chunk index and info

	BinaryIO::BinaryRead(stream, m_ChunkIndex);
	BinaryIO::BinaryRead(stream, m_Width);
	BinaryIO::BinaryRead(stream, m_Height);
	BinaryIO::BinaryRead(stream, m_X);
	BinaryIO::BinaryRead(stream, m_Z);

	m_HeightMap = new RawHeightMap(stream, m_Height, m_Width);

	// We can just skip the rest of the data so we can read the next chunks, we don't need anymore data

	uint32_t colorMapSize;
	BinaryIO::BinaryRead(stream, colorMapSize);
	stream.seekg((uint32_t)stream.tellg() + (colorMapSize * colorMapSize * 4));

	uint32_t lightmapSize;
	BinaryIO::BinaryRead(stream, lightmapSize);
	stream.seekg((uint32_t)stream.tellg() + (lightmapSize));

	uint32_t colorMapSize2;
	BinaryIO::BinaryRead(stream, colorMapSize2);
	stream.seekg((uint32_t)stream.tellg() + (colorMapSize2 * colorMapSize2 * 4));

	uint8_t unknown;
	BinaryIO::BinaryRead(stream, unknown);

	uint32_t blendmapSize;
	BinaryIO::BinaryRead(stream, blendmapSize);
	stream.seekg((uint32_t)stream.tellg() + (blendmapSize));

	uint32_t pointSize;
	BinaryIO::BinaryRead(stream, pointSize);
	stream.seekg((uint32_t)stream.tellg() + (pointSize * 9 * 4));

	stream.seekg((uint32_t)stream.tellg() + (colorMapSize * colorMapSize));

	uint32_t endCounter;
	BinaryIO::BinaryRead(stream, endCounter);
	stream.seekg((uint32_t)stream.tellg() + (endCounter * 2));

	if (endCounter != 0) {
		stream.seekg((uint32_t)stream.tellg() + (32));

		for (int i = 0; i < 0x10; i++) {
			uint16_t finalCountdown;
			BinaryIO::BinaryRead(stream, finalCountdown);
			stream.seekg((uint32_t)stream.tellg() + (finalCountdown * 2));
		}
	}

	// Generate our mesh/geo data for this chunk

	this->GenerateMesh();
}

RawChunk::~RawChunk() {
	if (m_Mesh) delete m_Mesh;
	if (m_HeightMap) delete m_HeightMap;
}

void RawChunk::GenerateMesh() {
	RawMesh* meshData = new RawMesh();

	for (int i = 0; i < m_Width; ++i) {
		for (int j = 0; j < m_Height; ++j) {
			float y = *std::next(m_HeightMap->m_FloatMap.begin(), m_Width * i + j);

			meshData->m_Vertices.push_back(NiPoint3(i, y, j));

			if (i == 0 || j == 0) continue;

			meshData->m_Triangles.push_back(m_Width * i + j);
			meshData->m_Triangles.push_back(m_Width * i + j - 1);
			meshData->m_Triangles.push_back(m_Width * (i - 1) + j - 1);

			meshData->m_Triangles.push_back(m_Width * (i - 1) + j - 1);
			meshData->m_Triangles.push_back(m_Width * (i - 1) + j);
			meshData->m_Triangles.push_back(m_Width * i + j);
		}
	}

	m_Mesh = meshData;
}
