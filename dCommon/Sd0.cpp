#include "Sd0.h"

#include <array>
#include <ranges>

#include "BinaryIO.h"

#include "Game.h"
#include "Logger.h"

#include "ZCompression.h"

// Insert header if on first buffer
void WriteHeader(Sd0::BinaryBuffer& chunk) {
	chunk.push_back(Sd0::SD0_HEADER[0]);
	chunk.push_back(Sd0::SD0_HEADER[1]);
	chunk.push_back(Sd0::SD0_HEADER[2]);
	chunk.push_back(Sd0::SD0_HEADER[3]);
	chunk.push_back(Sd0::SD0_HEADER[4]);
}

// Write the size of the buffer to a chunk
void WriteSize(Sd0::BinaryBuffer& chunk, uint32_t chunkSize) {
	for (int i = 0; i < 4; i++) {
		char toPush = chunkSize & 0xff;
		chunkSize = chunkSize >> 8;
		chunk.push_back(toPush);
	}
}

int32_t GetDataOffset(bool firstBuffer) {
	return firstBuffer ? 9 : 4;
}

Sd0::Sd0(std::istream& buffer) {
	char header[5]{};

	// Check if this is an sd0 buffer. It's possible we may be handed a zlib buffer directly due to old code so check for that too.
	if (!BinaryIO::BinaryRead(buffer, header) || memcmp(header, SD0_HEADER, sizeof(header)) != 0) {
		LOG("Failed to read SD0 header %i %i %i %i %i %i %i", buffer.good(), buffer.tellg(), header[0], header[1], header[2], header[3], header[4]);
		LOG_DEBUG("This may be a zlib buffer directly? Trying again assuming its a zlib buffer.");
		auto& firstChunk = m_Chunks.emplace_back();
		WriteHeader(firstChunk);
		buffer.seekg(0, std::ios::end);
		uint32_t bufferSize = buffer.tellg();
		buffer.seekg(0, std::ios::beg);
		WriteSize(firstChunk, bufferSize);
		firstChunk.resize(firstChunk.size() + bufferSize);
		auto* dataStart = reinterpret_cast<char*>(firstChunk.data() + GetDataOffset(true));
		if (!buffer.read(dataStart, bufferSize)) {
			m_Chunks.pop_back();
			LOG("Failed to read %u bytes from chunk %i", bufferSize, m_Chunks.size() - 1);
		}
		return;
	}

	while (buffer && buffer.peek() != std::istream::traits_type::eof()) {
		uint32_t chunkSize{};
		if (!BinaryIO::BinaryRead(buffer, chunkSize)) {
			LOG("Failed to read chunk size from stream %lld %zu", buffer.tellg(), m_Chunks.size());
			break;
		}
		auto& chunk = m_Chunks.emplace_back();
		bool firstBuffer = m_Chunks.size() == 1;
		auto dataOffset = GetDataOffset(firstBuffer);

		// Insert header if on first buffer
		if (firstBuffer) {
			WriteHeader(chunk);
		}

		WriteSize(chunk, chunkSize);

		chunk.resize(chunkSize + dataOffset);
		auto* dataStart = reinterpret_cast<char*>(chunk.data() + dataOffset);
		if (!buffer.read(dataStart, chunkSize)) {
			m_Chunks.pop_back();
			LOG("Failed to read %u bytes from chunk %i", chunkSize, m_Chunks.size() - 1);
			break;
		}
	}
}

void Sd0::FromData(const uint8_t* data, size_t bufferSize) {
	const auto originalBufferSize = bufferSize;
	if (bufferSize == 0) return;

	m_Chunks.clear();
	while (bufferSize > 0) {
		const auto numToCopy = std::min(MAX_UNCOMPRESSED_CHUNK_SIZE, bufferSize);
		const auto* startOffset = data + originalBufferSize - bufferSize;
		bufferSize -= numToCopy;
		std::array<uint8_t, MAX_UNCOMPRESSED_CHUNK_SIZE> compressedChunk;
		const auto compressedSize = ZCompression::Compress(
			startOffset, numToCopy,
			compressedChunk.data(), compressedChunk.size());

		auto& chunk = m_Chunks.emplace_back();
		bool firstBuffer = m_Chunks.size() == 1;
		auto dataOffset = GetDataOffset(firstBuffer);

		if (firstBuffer) {
			WriteHeader(chunk);
		}

		WriteSize(chunk, compressedSize);

		chunk.resize(compressedSize + dataOffset);
		memcpy(chunk.data() + dataOffset, compressedChunk.data(), compressedSize);
	}

}

std::string Sd0::GetAsStringUncompressed() const {
	std::string toReturn;
	bool first = true;
	uint32_t totalSize{};
	for (const auto& chunk : m_Chunks) {
		auto dataOffset = GetDataOffset(first);
		first = false;
		const auto chunkSize = chunk.size();

		auto oldSize = toReturn.size();
		toReturn.resize(oldSize + MAX_UNCOMPRESSED_CHUNK_SIZE);
		int32_t error{};
		const auto uncompressedSize = ZCompression::Decompress(
			chunk.data() + dataOffset, chunkSize - dataOffset,
			reinterpret_cast<uint8_t*>(toReturn.data()) + oldSize, MAX_UNCOMPRESSED_CHUNK_SIZE,
			error);

		totalSize += uncompressedSize;
	}

	toReturn.resize(totalSize);
	return toReturn;
}

std::stringstream Sd0::GetAsStream() const {
	std::stringstream toReturn;

	for (const auto& chunk : m_Chunks) {
		toReturn.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
	}

	return toReturn;
}

const std::vector<Sd0::BinaryBuffer>& Sd0::GetAsVector() const {
	return m_Chunks;
}
