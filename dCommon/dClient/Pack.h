#pragma once

#include <vector>
#include <string>
#include <filesystem>

#pragma pack(push, 1)
struct PackRecord {
	uint32_t m_Crc;
	int32_t m_LowerCrc;
	int32_t m_UpperCrc;
	uint32_t m_UncompressedSize;
	char m_UncompressedHash[32];
	uint32_t m_Padding1;
	uint32_t m_CompressedSize;
	char m_CompressedHash[32];
	uint32_t m_Padding2;
	uint32_t m_FilePointer;
	uint32_t m_IsCompressed; // u32 bool
};
#pragma pack(pop)

class Pack {
public:
	Pack(const std::filesystem::path& filePath);
	~Pack() = default;

	bool HasFile(uint32_t crc);
	bool ReadFileFromPack(uint32_t crc, char** data, uint32_t* len);
private:
	std::ifstream m_FileStream;
	std::filesystem::path m_FilePath;

	char m_Version[7];

	uint32_t m_RecordCount;
	std::vector<PackRecord> m_Records;
};
