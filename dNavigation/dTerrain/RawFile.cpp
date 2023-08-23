#include "RawFile.h"

#include "BinaryIO.h"
#include "RawChunk.h"
#include "RawMesh.h"
#include "RawHeightMap.h"

RawFile::RawFile(std::string fileName) {
	if (!BinaryIO::DoesFileExist(fileName)) return;

	std::ifstream file(fileName, std::ios::binary);

	// Read header

	BinaryIO::BinaryRead(file, m_Version);
	BinaryIO::BinaryRead(file, m_Padding);
	BinaryIO::BinaryRead(file, m_ChunkCount);
	BinaryIO::BinaryRead(file, m_Width);
	BinaryIO::BinaryRead(file, m_Height);


	if (m_Version < 0x20) {
		return; // Version too crusty to handle
	}

	// Read in chunks

	m_Chunks = {};

	for (uint32_t i = 0; i < m_ChunkCount; i++) {
		RawChunk* chunk = new RawChunk(file);
		m_Chunks.push_back(chunk);
	}

	m_FinalMesh = new RawMesh();

	this->GenerateFinalMeshFromChunks();
}

RawFile::~RawFile() {
	if (m_FinalMesh) delete m_FinalMesh;
	for (const auto* item : m_Chunks) {
		if (item) delete item;
	}
}

void RawFile::GenerateFinalMeshFromChunks() {
	uint32_t lenOfLastChunk = 0; // index of last vert set in the last chunk

	for (const auto& chunk : m_Chunks) {
		for (const auto& vert : chunk->m_Mesh->m_Vertices) {
			auto tempVert = vert;

			// Scale X and Z by the chunk's position in the world
			// Scale Y by the chunk's heightmap scale factor
			tempVert.SetX(tempVert.GetX() + (chunk->m_X / chunk->m_HeightMap->m_ScaleFactor));
			tempVert.SetY(tempVert.GetY() / chunk->m_HeightMap->m_ScaleFactor);
			tempVert.SetZ(tempVert.GetZ() + (chunk->m_Z / chunk->m_HeightMap->m_ScaleFactor));

			// Then scale it again for some reason
			tempVert *= chunk->m_HeightMap->m_ScaleFactor;

			m_FinalMesh->m_Vertices.push_back(tempVert);
		}

		for (const auto& tri : chunk->m_Mesh->m_Triangles) {
			m_FinalMesh->m_Triangles.push_back(tri + lenOfLastChunk);
		}

		lenOfLastChunk += chunk->m_Mesh->m_Vertices.size();
	}
}

void RawFile::WriteFinalMeshToOBJ(std::string path) {
	std::ofstream file(path);

	for (const auto& v : m_FinalMesh->m_Vertices) {
		file << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
	}

	for (int i = 0; i < m_FinalMesh->m_Triangles.size(); i += 3) {
		file << "f " << *std::next(m_FinalMesh->m_Triangles.begin(), i) + 1 << ' ' << *std::next(m_FinalMesh->m_Triangles.begin(), i + 1) + 1 << ' ' << *std::next(m_FinalMesh->m_Triangles.begin(), i + 2) + 1 << '\n';
	}
}
