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

	[[nodiscard]]
	const std::vector<std::string>& GetPackPaths() const { return m_PackPaths; }

	[[nodiscard]]
	const std::vector<PackFileIndex>& GetPackFileIndices() const { return m_PackFileIndices; }

	[[nodiscard]]
	const std::vector<Pack>& GetPacks() const { return m_Packs; }
private:
	std::ifstream m_FileStream;

	uint32_t m_Version;

	std::vector<std::string> m_PackPaths;

	std::vector<PackFileIndex> m_PackFileIndices;

	std::vector<Pack> m_Packs;
};
