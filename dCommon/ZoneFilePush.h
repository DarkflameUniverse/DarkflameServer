#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>

#include "ZoneFilePack.h"
#include "ZonePublish.h"

/**
 * Taking a zone's files as the game client's level editor sends them (ZoneFilePack) into the zone's set: unpacked,
 * made into a whole set, checked like any other set (ZonePublish) and applied (ZoneOverrideStore), or refused with
 * nothing written. A whole map is the zone's new set. One file replaces that file in the set the zone has and keeps
 * the rest; a set always has the zone's .luz, the client's own when none was sent or kept.
 *
 * Only reads the bytes it is given and files under the override folder: no connection, database or setting, so it
 * runs on a worker thread.
 */
namespace ZoneFilePush {
	struct Job {
		uint32_t zoneId{};
		std::string luzPath;                  // from the ZoneTable, relative to res/maps/, lowercase
		ZoneFilePack::eFileType fileType{};
		std::string packedFiles;
		ZonePublish::Limits limits;
		std::filesystem::path root;           // the override folder
		std::string id;                       // of this change (ZoneOverrideStore::ValidId)
		std::string who;
		int64_t now{};
		uint32_t keepBackups = 20;
		std::set<std::string> clientScenes;   // the scenes the zone has in the client's files
		std::set<std::string> clientFiles;    // which of the files a set can leave out the client has
		std::optional<std::string> clientLuz; // the client's own zone file
	};

	struct Result {
		bool applied{};
		std::string problem; // why not, for the sender
		size_t files{};      // in the zone's set now
		std::optional<std::string> backupId;
	};

	Result Run(const Job& job);
}
