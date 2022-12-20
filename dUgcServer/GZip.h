#pragma once

#include <cstdint>

#include <string>

// gaming.h

enum class eGZipFlag : uint8_t {
	FText = 0,
	FHCrc,
	FExtra,
	FName,
	FComment,
};

enum class eGZipOs : uint8_t {
	NT = 0,
	Amiga,
	VMS,
	Unix,
	CMS,
	Atari,
	HPFS,
	Macintosh,
	ZSystem,
	CMP,
	TOPS20,
	NTFS,
	QDOS,
	AcornRiscos,
	Unknown = 255
};

class GZip {
public:
	GZip(std::string filename, void* buffer, uint8_t size);
	~GZip();

	void WriteToFile(std::string outName);
private:
	// Constants
	static constexpr uint32_t SizeOfHeader = 10;
	static constexpr uint32_t SizeOfFooter = 8;
	static constexpr uint16_t FileMagic = 0x8B1F;

	size_t CalculateSizeOfGZip() {
		return SizeOfHeader + m_FileName.length() + 1 + m_CompressedSize + SizeOfFooter;
	}

	void SetFlag(eGZipFlag flag, bool value) {
		if (value) m_Flags |= 1 << (uint8_t)flag;
		else m_Flags &= ~(1 << (uint8_t)flag);
	}

	void* m_FileData;
	uint32_t m_FileSize;
	
	uint32_t m_CompressedSize;

	// https://www.ietf.org/rfc/rfc1952.txt
	
	// Header
	uint16_t m_ID = FileMagic;
	
	uint8_t m_CompressionLevel = 0;
	uint8_t m_Flags = 0;
	uint32_t m_MTime = 0;
	uint8_t m_CompressionFlags = 0;
	eGZipOs m_OperatingSystem = eGZipOs::Unknown;

	// Optional Data
	std::string m_FileName;

	// Footer
	uint32_t m_Crc;
	uint32_t m_InputSize;
};
