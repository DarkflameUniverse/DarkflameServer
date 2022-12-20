#include "GZip.h"

#include <zlib.h>

#include "BinaryIO.h"
#include "ZCompression.h"

GZip::GZip(std::string filename, void* buffer, uint8_t size) {
	m_FileName = filename;
	m_InputSize = size;

	size_t compressTheorySize = (size_t)ZCompression::GetMaxCompressedLength(size);
	void* compressOut = malloc(compressTheorySize);

	m_CompressedSize = ZCompression::GZipCompress((uint8_t*)buffer, size, (uint8_t*)compressOut, compressTheorySize);

	m_FileData = malloc(CalculateSizeOfGZip());

	std::memcpy(reinterpret_cast<void*>((size_t)m_FileData + (SizeOfHeader + m_FileName.length() + 1)), compressOut, m_CompressedSize);
	free(compressOut);

	// Configure File information
	m_CompressionLevel = 8;
	m_OperatingSystem = eGZipOs::Unknown;
	SetFlag(eGZipFlag::FName, true);
	m_MTime = time(NULL);

	*reinterpret_cast<uint16_t*>(m_FileData) = m_ID;
	*reinterpret_cast<uint8_t*>((size_t)m_FileData + 2) = m_CompressionLevel;
	*reinterpret_cast<uint8_t*>((size_t)m_FileData + 3) = m_Flags;
	*reinterpret_cast<uint32_t*>((size_t)m_FileData + 4) = m_MTime;
	*reinterpret_cast<uint8_t*>((size_t)m_FileData + 8) = m_CompressionFlags;
	*reinterpret_cast<uint8_t*>((size_t)m_FileData + 9) = (uint8_t)m_OperatingSystem;

	for (int i = 0; i < m_FileName.length(); i++) {
		*reinterpret_cast<char*>((size_t)m_FileData + SizeOfHeader + i) = m_FileName[i];
	}
	*reinterpret_cast<uint8_t*>((size_t)m_FileData + (SizeOfHeader + m_FileName.length())) = 0x00;

	m_Crc = crc32(0, Z_NULL, 0);
	m_Crc = crc32(m_Crc, (uint8_t*)buffer, size);

	*reinterpret_cast<uint32_t*>((size_t)m_FileData + (CalculateSizeOfGZip() - 8)) = m_Crc;
	*reinterpret_cast<uint32_t*>((size_t)m_FileData + (CalculateSizeOfGZip() - 4)) = m_InputSize;

	m_FileSize = CalculateSizeOfGZip();
}

void GZip::WriteToFile(std::string outName) {
	std::ofstream file(outName);

	file.write((const char*)m_FileData, m_FileSize);
	file.close();
}

GZip::~GZip() {
	delete m_FileData;
}

