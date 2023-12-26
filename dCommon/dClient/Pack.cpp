#include "Pack.h"

#include "BinaryIO.h"
#include "ZCompression.h"

Pack::Pack(const std::filesystem::path& filePath) {
	m_FilePath = filePath;

	if (!std::filesystem::exists(filePath)) {
		return;
	}

	m_FileStream = std::ifstream(filePath, std::ios::in | std::ios::binary);

	m_FileStream.read(m_Version, 7);

	m_FileStream.seekg(-8, std::ios::end); // move file pointer to 8 bytes before the end (location of the address of the record count)

	uint32_t recordCountPos = 0;
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, recordCountPos);

	m_FileStream.seekg(recordCountPos, std::ios::beg);

	BinaryIO::BinaryRead<uint32_t>(m_FileStream, m_RecordCount);

	for (int i = 0; i < m_RecordCount; i++) {
		PackRecord record;
		BinaryIO::BinaryRead<PackRecord>(m_FileStream, record);

		m_Records.push_back(record);
	}

	m_FileStream.close();
}

bool Pack::HasFile(uint32_t crc) {
	for (const auto& record : m_Records) {
		if (record.m_Crc == crc) {
			return true;
		}
	}

	return false;
}

bool Pack::ReadFileFromPack(uint32_t crc, char** data, uint32_t* len) {
	// Time for some wacky C file reading for speed reasons

	PackRecord pkRecord{};

	for (const auto& record : m_Records) {
		if (record.m_Crc == crc) {
			pkRecord = record;
			break;
		}
	}

	if (pkRecord.m_Crc == 0) return false;

	size_t pos = 0;
	pos += pkRecord.m_FilePointer;

	bool isCompressed = (pkRecord.m_IsCompressed & 0xff) > 0;
	auto inPackSize = isCompressed ? pkRecord.m_CompressedSize : pkRecord.m_UncompressedSize;

	FILE* file;
#ifdef _WIN32
	fopen_s(&file, m_FilePath.string().c_str(), "rb");
#elif __APPLE__
	// macOS has 64bit file IO by default
	file = fopen(m_FilePath.string().c_str(), "rb");
#else
	file = fopen64(m_FilePath.string().c_str(), "rb");
#endif

	fseek(file, pos, SEEK_SET);

	if (!isCompressed) {
		char* tempData = static_cast<char*>(malloc(pkRecord.m_UncompressedSize));
		int32_t readInData = fread(tempData, sizeof(uint8_t), pkRecord.m_UncompressedSize, file);

		*data = tempData;
		*len = pkRecord.m_UncompressedSize;
		fclose(file);

		return true;
	}

	pos += 5; // skip header

	fseek(file, pos, SEEK_SET);

	char* decompressedData = static_cast<char*>(malloc(pkRecord.m_UncompressedSize));
	uint32_t currentReadPos = 0;

	while (true) {
		if (currentReadPos >= pkRecord.m_UncompressedSize) break;

		uint32_t size;
		int32_t readInData = fread(&size, sizeof(uint32_t), 1, file);
		pos += 4; // Move pointer position 4 to the right

		char* chunk = static_cast<char*>(malloc(size));
		int32_t readInData2 = fread(chunk, sizeof(int8_t), size, file);
		pos += size; // Move pointer position the amount of bytes read to the right

		int32_t err;
		currentReadPos += ZCompression::Decompress(reinterpret_cast<uint8_t*>(chunk), size, reinterpret_cast<uint8_t*>(decompressedData + currentReadPos), ZCompression::MAX_SD0_CHUNK_SIZE, err);

		free(chunk);
	}

	*data = decompressedData;
	*len = pkRecord.m_UncompressedSize;

	fclose(file);

	return true;
}
