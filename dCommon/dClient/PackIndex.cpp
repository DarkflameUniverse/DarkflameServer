#include "PackIndex.h"


PackIndex::PackIndex(const std::filesystem::path& filePath) {
	m_FileStream = std::ifstream(filePath / "versions" / "primary.pki", std::ios::in | std::ios::binary);

	BinaryIO::BinaryRead<uint32_t>(m_FileStream, m_Version);
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, m_PackPathCount);

	for (int i = 0; i < m_PackPathCount; i++) {
		uint32_t stringLen = 0;
		BinaryIO::BinaryRead<uint32_t>(m_FileStream, stringLen);

		std::string path;

		for (int j = 0; j < stringLen; j++) {
			char inChar;
			BinaryIO::BinaryRead<char>(m_FileStream, inChar);

			path += inChar;
		}

		m_PackPaths.push_back(path);
	}

	BinaryIO::BinaryRead<uint32_t>(m_FileStream, m_PackFileIndexCount);

	for (int i = 0; i < m_PackFileIndexCount; i++) {
		PackFileIndex packFileIndex;
		BinaryIO::BinaryRead<PackFileIndex>(m_FileStream, packFileIndex);

		m_PackFileIndices.push_back(packFileIndex);
	}

	Game::logger->Log("PackIndex", "Loaded pack catalog with %i pack files and %i files", m_PackPaths.size(), m_PackFileIndices.size());

	for (const auto& item : m_PackPaths) {
		auto* pack = new Pack(filePath / item);

		m_Packs.push_back(pack);
	}
}
