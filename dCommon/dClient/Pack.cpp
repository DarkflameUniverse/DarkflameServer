#include "Pack.h"

#include "BinaryIO.h"
#include "Sd0.h"
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

	uint32_t recordCount = 0;
	BinaryIO::BinaryRead<uint32_t>(m_FileStream, recordCount);

	m_Records.reserve(recordCount);
	std::generate_n(std::back_inserter(m_Records), recordCount, [&] {
		PackRecord record;
		BinaryIO::BinaryRead<PackRecord>(m_FileStream, record);
		return record;
	});

	m_FileStream.close();
}

bool Pack::HasFile(const uint32_t crc) const {
	for (const auto& record : m_Records) {
		if (record.m_Crc == crc) {
			return true;
		}
	}

	return false;
}

bool Pack::ReadFileFromPack(const uint32_t crc, char** data, uint32_t* len) const {
	const auto pathStr = m_FilePath.string();
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

	FILE* file = nullptr;
#ifdef _WIN32
	fopen_s(&file, pathStr.c_str(), "rb");
#elif __APPLE__
	// macOS has 64bit file IO by default
	file = fopen(pathStr.c_str(), "rb");
#else
	file = fopen64(pathStr.c_str(), "rb");
#endif

	if (!file) {
		LOG("No file found for path %s", pathStr.c_str());
		throw std::runtime_error("Could not find file " + pathStr);
	}

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

		std::unique_ptr<char[]> chunk(new char[size]);
		int32_t readInData2 = fread(chunk.get(), sizeof(int8_t), size, file);
		pos += size; // Move pointer position the amount of bytes read to the right

		int32_t err;
		const auto countToRead = ZCompression::Decompress(reinterpret_cast<uint8_t*>(chunk.get()), size, reinterpret_cast<uint8_t*>(decompressedData + currentReadPos), Sd0::MAX_UNCOMPRESSED_CHUNK_SIZE, err);
		if (countToRead == -1) {
			LOG("Error decompressing zlib data from file %s", pathStr.c_str());
			throw std::runtime_error("Error decompressing zlib data from file " + pathStr);
		}
		currentReadPos += countToRead;

	}

	*data = decompressedData;
	*len = pkRecord.m_UncompressedSize;

	fclose(file);

	return true;
}
