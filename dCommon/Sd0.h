// Darkflame Universe
// Copyright 2025

#ifndef SD0_H
#define SD0_H

#include <fstream>
#include <vector>

// Sd0 is comprised of multiple zlib compressed buffers stored in a row.
// The format starts with a SD0 header (see SD0_HEADER) followed by the size of a zlib buffer, and then the zlib buffer itself.
// This repeats until end of file
class Sd0 {
public:
	using BinaryBuffer = std::vector<uint8_t>;

	static inline const char* SD0_HEADER = "sd0\x01\xff";

	/**
	 * @brief Max size of an inflated sd0 zlib chunk
	 */
	static constexpr inline size_t MAX_UNCOMPRESSED_CHUNK_SIZE = 1024 * 256;

	// Read the input buffer into an internal chunk stream to be used later
	Sd0(std::istream& buffer);

	// Uncompresses the entire Sd0 buffer and returns it as a string
	[[nodiscard]] std::string GetAsStringUncompressed() const;

	// Gets the Sd0 buffer as a stream in its raw compressed form
	[[nodiscard]] std::stringstream GetAsStream() const;

	// Gets the Sd0 buffer as a vector in its raw compressed form
	[[nodiscard]] const std::vector<BinaryBuffer>& GetAsVector() const;

	// Compress data into a Sd0 buffer
	void FromData(const uint8_t* data, size_t bufferSize);
private:
	std::vector<BinaryBuffer> m_Chunks{};
};

#endif //!SD0_H
