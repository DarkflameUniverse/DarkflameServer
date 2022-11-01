#pragma once

#include <cstdint>

#include <string>
#include <vector>
#include <filesystem>

#include "Pack.h"

#pragma pack(push, 1)
struct PackFileIndex {
	uint32_t m_Crc;
	int32_t m_LowerCrc;
	int32_t m_UpperCrc;
	uint32_t m_PackFileIndex;
	uint32_t m_IsCompressed; // u32 bool?
};
#pragma pack(pop)

class PackIndex {
public:
	PackIndex(const std::filesystem::path& filePath);
	~PackIndex();

	const std::vector<std::string>& GetPackPaths() { return m_PackPaths; }
	const std::vector<PackFileIndex>& GetPackFileIndices() { return m_PackFileIndices; }
	const std::vector<Pack*>& GetPacks() { return m_Packs; }
private:
	std::ifstream m_FileStream;

	uint32_t m_Version;

	uint32_t m_PackPathCount;
	std::vector<std::string> m_PackPaths;
	uint32_t m_PackFileIndexCount;
	std::vector<PackFileIndex> m_PackFileIndices;

	std::vector<Pack*> m_Packs;
};
