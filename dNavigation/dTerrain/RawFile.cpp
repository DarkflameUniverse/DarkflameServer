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

	this->GenerateFinalMeshFromChunks();
}

RawFile::~RawFile() {
	delete m_FinalMesh;
	for (const auto* item : m_Chunks) {
		delete item;
	}
}

void RawFile::GenerateFinalMeshFromChunks() {
	uint32_t lenOfLastChunk = 0; // index of last vert set in the last chunk

	for (const auto& chunk : m_Chunks) {
		for (const auto& vert : chunk->m_Mesh->m_Vertices) {
			auto tempVert = vert;

			tempVert.SetX(tempVert.GetX() + (chunk->m_X / 4));
			tempVert.SetZ(tempVert.GetZ() + (chunk->m_Z / 4));

			tempVert* chunk->m_HeightMap->m_ScaleFactor;

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
	std::string vertData;

	for (const auto& v : m_FinalMesh->m_Vertices) {
		vertData += "v " + std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z) + "\n";
	}

	for (int i = 0; i < m_FinalMesh->m_Triangles.size(); i += 3) {
		vertData += "f " + std::to_string(*std::next(m_FinalMesh->m_Triangles.begin(), i) + 1) + " " + std::to_string(*std::next(m_FinalMesh->m_Triangles.begin(), i + 1) + 1) + " " + std::to_string(*std::next(m_FinalMesh->m_Triangles.begin(), i + 2) + 1) + "\n";
	}

	file.write(vertData.c_str(), vertData.size());
	file.close();
}
