#include "Zip.h"

#include "ZCompression.h"

#include <cstdint>
#include <cstring>

namespace {
	constexpr uint32_t END_SIGNATURE = 0x06054b50;
	constexpr uint32_t CENTRAL_SIGNATURE = 0x02014b50;
	constexpr uint32_t LOCAL_SIGNATURE = 0x04034b50;
	constexpr uint64_t END_SIZE = 22;
	constexpr uint64_t MAX_COMMENT_SIZE = 65535;

	// Zip numbers are little endian
	template<typename T>
	bool ReadAt(const std::string_view data, const uint64_t offset, T& value) {
		if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
		value = 0;
		for (uint64_t i = 0; i < sizeof(T); i++) value |= static_cast<T>(static_cast<uint8_t>(data[offset + i])) << (8 * i);
		return true;
	}
}

bool Zip::ForEachEntry(const std::string_view zip, const std::function<bool(std::string_view name)>& wanted,
	const std::function<void(std::string_view name, std::string_view data)>& visit) {
	if (zip.size() < END_SIZE) return false;

	// The end of central directory record: the last one in the file, after which only its comment can come
	uint64_t end = 0;
	bool found = false;
	const uint64_t lowest = zip.size() > END_SIZE + MAX_COMMENT_SIZE ? zip.size() - END_SIZE - MAX_COMMENT_SIZE : 0;
	for (uint64_t i = zip.size() - END_SIZE + 1; i-- > lowest;) {
		uint32_t signature{};
		if (ReadAt(zip, i, signature) && signature == END_SIGNATURE) {
			end = i;
			found = true;
			break;
		}
	}
	if (!found) return false;

	uint16_t entries{};
	uint32_t directoryOffset{};
	if (!ReadAt(zip, end + 10, entries) || !ReadAt(zip, end + 16, directoryOffset)) return false;

	uint64_t offset = directoryOffset;
	for (uint16_t i = 0; i < entries; i++) {
		uint32_t signature{}, compressedSize{}, size{}, localOffset{};
		uint16_t method{}, nameLength{}, extraLength{}, commentLength{};
		if (!ReadAt(zip, offset, signature) || signature != CENTRAL_SIGNATURE || !ReadAt(zip, offset + 10, method) ||
			!ReadAt(zip, offset + 20, compressedSize) || !ReadAt(zip, offset + 24, size) || !ReadAt(zip, offset + 28, nameLength) ||
			!ReadAt(zip, offset + 30, extraLength) || !ReadAt(zip, offset + 32, commentLength) || !ReadAt(zip, offset + 42, localOffset) ||
			offset + 46 + nameLength > zip.size()) {
			return false;
		}
		const auto name = zip.substr(offset + 46, nameLength);
		offset += 46 + nameLength + extraLength + commentLength;
		if (!wanted(name)) continue;

		// The data follows the local header, whose name and extra field can differ in length from the central one's
		uint16_t localNameLength{}, localExtraLength{};
		if (!ReadAt(zip, localOffset, signature) || signature != LOCAL_SIGNATURE || !ReadAt(zip, localOffset + 26, localNameLength) ||
			!ReadAt(zip, localOffset + 28, localExtraLength)) {
			continue;
		}
		const uint64_t dataStart = static_cast<uint64_t>(localOffset) + 30 + localNameLength + localExtraLength;
		if (dataStart > zip.size() || compressedSize > zip.size() - dataStart) continue;
		const auto data = zip.substr(dataStart, compressedSize);
		if (method == 0) {
			visit(name, data);
		} else if (method == 8) {
			if (const auto inflated = ZCompression::InflateRaw(data, size)) visit(name, *inflated);
		}
	}
	return true;
}
