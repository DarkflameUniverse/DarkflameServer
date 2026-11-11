#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * A multipart/form-data body that is in a file (Web's upload routes write large bodies to disk): its parts are split
 * out without ever holding the body in memory. A file part's bytes go to a new file in the folder given, named by
 * its position (never by the name the sender gave it, which is only handed back as text); a field's value is kept
 * as text. No sockets and no settings, so it runs on a worker thread and is unit tested.
 */
namespace MultipartFile {
	struct Part {
		std::string name;           // the form field's name
		std::string filename;       // as sent, unchecked; empty for a field
		bool isFile{};
		std::string value;          // a field's value
		std::filesystem::path file; // a file part's bytes
		uint64_t bytes{};           // a file part's size as sent
		bool tooLarge{};            // a file part over maxFileBytes: `file` holds only its start
	};

	struct Limits {
		uint32_t maxParts = 160;
		uint64_t maxFileBytes = 64ULL * 1024 * 1024;
		uint64_t maxFieldBytes = 64 * 1024;
		uint64_t maxHeaderBytes = 8 * 1024;
	};

	enum class eError : uint8_t {
		NONE,
		MALFORMED,       // not a multipart body with this boundary, or it ends early
		TOO_MANY_PARTS,
		FIELD_TOO_LARGE,
		IO,              // the body couldn't be read or a part couldn't be written
	};

	// The boundary of a "multipart/form-data; boundary=..." content type; nullopt for anything else
	std::optional<std::string> Boundary(std::string_view contentType);

	eError Split(const std::filesystem::path& body, const std::string& boundary, const std::filesystem::path& outDir, const Limits& limits, std::vector<Part>& parts);
}
