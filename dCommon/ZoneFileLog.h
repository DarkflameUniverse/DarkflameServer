#ifndef ZONEFILELOG_H
#define ZONEFILELOG_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class AssetManager;
struct AssetStream;

/**
 * The zone data files a world server loaded (docs/WorldHotReload.md): the .luz, every .lvl scene and its .lutriggers,
 * the terrain .raw and the navmesh, each with its size and a hash of the bytes the world read. The world sends the
 * list to master once it is ready (WORLD_FILES), and master watches those files for changes.
 *
 * Main thread only (the world records while it loads its zone).
 */
namespace ZoneFileLog {
	enum class eKind : uint8_t {
		ZONE = 0,     // .luz
		SCENE,        // .lvl
		TRIGGERS,     // .lutriggers
		TERRAIN,      // .raw
		NAVMESH,      // navmeshes/<zone>.bin
		OTHER,
	};

	const char* KindName(eKind kind);

	struct Entry {
		eKind kind{ eKind::OTHER };
		bool packed{};       // read from the client's packs: path is the name in the pack, and nothing watches it
		uint64_t size{};
		uint64_t hash{};     // FdbSnapshot::Hash of the bytes read
		std::string path;    // absolute path of a loose file

		bool operator==(const Entry& other) const = default;
	};

	// A file the world read; a path already recorded is recorded again (the last read wins)
	void Record(eKind kind, const std::string& path, bool packed, std::string_view bytes);

	// A file read from the client's res folder through the asset manager: the loose file's absolute path, or the
	// name in the pack
	void RecordAsset(const AssetManager* assets, eKind kind, const std::string& name, const AssetStream& stream);

	// A file read straight from disk (the navmesh): hashed here, not recorded when it can't be read
	void RecordFile(eKind kind, const std::filesystem::path& path);

	const std::vector<Entry>& Entries();

	void Clear();
};

#endif // ZONEFILELOG_H
