#include "PackIndex.h"
#include "BinaryIO.h"
#include "Game.h"
#include "Logger.h"

PackIndex::PackIndex(const std::filesystem::path& filePath) {
	m_FileStream = std::ifstream(filePath / "versions" / "primary.pki", std::ios::in | std::ios::binary);

	uint32_t packPathCount = 0;
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, m_Version);
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, packPathCount);
	
	m_PackPaths.resize(packPathCount);
	for (auto& item : m_PackPaths) {
		BinaryIO::ReadString<uint32_t>(m_FileStream, item, BinaryIO::ReadType::String);
	}

	uint32_t packFileIndexCount = 0;
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, packFileIndexCount);

	m_PackFileIndices.reserve(packFileIndexCount);
	std::generate_n(std::back_inserter(m_PackFileIndices), packFileIndexCount, [&] {
		PackFileIndex packFileIndex;
		BinaryIO::BinaryRead<PackFileIndex>(m_FileStream, packFileIndex);
		return packFileIndex;
	});

	LOG("Loaded pack catalog with %i pack files and %i files", m_PackPaths.size(), m_PackFileIndices.size());

	m_Packs.reserve(m_PackPaths.size());
	for (auto& item : m_PackPaths) {
		std::replace(item.begin(), item.end(), '\\', '/');
		m_Packs.emplace_back(filePath / item);
	}

	m_FileStream.close();
}
