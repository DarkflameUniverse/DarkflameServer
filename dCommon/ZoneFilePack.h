#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * A zone's files as the game client's level editor sends and takes them in one packet (docs/ZonePublish.md): a
 * list of named entries whose data is sd0 (zlib chunks of at most 256 KiB each).
 *
 *   entry:      u32 nameBytes (UTF-16LE with its null, so (characters + 1) * 2), u32 dataBytes (as compressed),
 *               the name, the data. No padding.
 *   whole map:  the zone (.luz) entry, the terrain (.raw) entry, u16 N and N scene (.lvl) entries, u16 M and M
 *               trigger (.lutriggers) entries
 *   one file:   exactly one entry (a scene, the terrain, the zone, or a scene's triggers)
 *
 * There is no checksum and no total size. A name is only ever a base name here: the folder part of what was sent is
 * dropped, as the client does with what it is sent, and a name is never used as a path. Pure: bytes in, bytes out,
 * with every length checked against what is left; it runs on a worker thread.
 */
namespace ZoneFilePack {
	enum class eFileType : uint8_t {
		WHOLE_MAP = 0,
		SCENE = 1,
		TERRAIN = 2,
		ZONE = 3,
		TRIGGERS = 4,
	};

	inline bool ValidType(uint8_t type) { return type <= static_cast<uint8_t>(eFileType::TRIGGERS); }

	struct Entry {
		std::string name; // the base name, as sent (ASCII)
		std::string data; // uncompressed
	};

	struct Files {
		std::optional<Entry> zone;
		std::optional<Entry> terrain;
		std::vector<Entry> scenes;
		std::vector<Entry> triggers;

		size_t Count() const { return (zone ? 1 : 0) + (terrain ? 1 : 0) + scenes.size() + triggers.size(); }
	};

	struct Limits {
		uint64_t maxFileBytes = 64ULL * 1024 * 1024;   // one file, uncompressed
		uint64_t maxTotalBytes = 256ULL * 1024 * 1024; // all files, uncompressed
		uint32_t maxFiles = 128;
	};

	enum class eError : uint8_t {
		NONE,
		TRUNCATED,        // a length runs past the end, or bytes are left over
		BAD_NAME,         // not UTF-16 with its null, not ASCII, or no usable base name (empty, ".", "..")
		NOT_SD0,          // an entry's data does not start with the sd0 header
		BAD_CHUNK,        // a chunk that is not a zlib stream, or one that inflates to more than 256 KiB
		TOO_LARGE,        // over a limit
		INCOMPLETE,       // a whole map without its zone, its terrain or a scene's data
	};

	const char* Describe(eError error);

	// The base name of a sent name: what follows its last '/' or '\'. nullopt for one that is empty, "." or "..",
	// or has a character that is not printable ASCII or that file names can't have.
	std::optional<std::string> BaseName(std::string_view name);

	// sd0 to bytes, strictly: the header, then chunks that each inflate to at most 256 KiB, at most maxBytes in all
	eError Inflate(std::string_view sd0, uint64_t maxBytes, std::string& out);

	// What follows the file type and level file ID in a push (or a pull response)
	eError Unpack(eFileType type, std::string_view body, const Limits& limits, Files& out);

	/**
	 * The same bytes from files, their data compressed to sd0. For one file the entry is the one of its kind (the
	 * first scene or trigger entry); empty when it is missing.
	 */
	std::string Pack(eFileType type, const Files& files);
}
