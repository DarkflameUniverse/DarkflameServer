#ifndef __WORLDFILES__H__
#define __WORLDFILES__H__

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "InstanceMigration.h"
#include "MessageType/Master.h"
#include "ZoneFileLog.h"
#include "dCommonVars.h"

/**
 * World hot reload (docs/WorldHotReload.md).
 */
namespace WorldFiles {
	constexpr uint16_t MAX_PATH = 1024;
	constexpr uint16_t MAX_FILES = 2048;
	constexpr uint16_t MAX_BY = 64;
	constexpr uint16_t MAX_MESSAGE = 256;

	inline void WriteFile(RakNet::BitStream& stream, const ZoneFileLog::Entry& file) {
		stream.Write(static_cast<uint8_t>(file.kind));
		stream.Write<uint8_t>(file.packed ? 1 : 0);
		stream.Write(file.size);
		stream.Write(file.hash);
		InstanceMigration::WriteText(stream, file.path, MAX_PATH);
	}

	inline bool ReadFile(RakNet::BitStream& stream, ZoneFileLog::Entry& file) {
		uint8_t kind{}, packed{};
		if (!stream.Read(kind) || !stream.Read(packed) || !stream.Read(file.size) || !stream.Read(file.hash)) return false;
		if (kind > static_cast<uint8_t>(ZoneFileLog::eKind::OTHER)) return false;
		file.kind = static_cast<ZoneFileLog::eKind>(kind);
		file.packed = packed != 0;
		return InstanceMigration::ReadText(stream, file.path, MAX_PATH) && !file.path.empty();
	}
}

/**
 * WORLD_FILES (world -> master, once the world is ready): the zone data files it loaded (ZoneFileLog). Master takes
 * the zone, instance and clone from the instance it knows at that address; the IDs here are for the log.
 */
struct WorldFilesReport : public LUBitStream {
	WorldFilesReport() : LUBitStream(ServiceType::MASTER, MessageType::Master::WORLD_FILES) {}

	uint32_t zoneId{};
	uint32_t instanceId{};
	uint32_t cloneId{};
	std::vector<ZoneFileLog::Entry> files;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(zoneId);
		stream.Write(instanceId);
		stream.Write(cloneId);
		const auto count = static_cast<uint16_t>(std::min<size_t>(files.size(), WorldFiles::MAX_FILES));
		stream.Write(count);
		for (uint16_t i = 0; i < count; i++) WorldFiles::WriteFile(stream, files[i]);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint16_t count{};
		if (!stream.Read(zoneId) || !stream.Read(instanceId) || !stream.Read(cloneId) || !stream.Read(count)) return false;
		if (count > WorldFiles::MAX_FILES) return false;
		files.assign(count, {});
		for (auto& file : files) {
			if (!WorldFiles::ReadFile(stream, file)) return false;
		}
		return true;
	}
};

/**
 * WORLD_RELOAD (world for a GM's /reloadworld, or the dashboard -> master): replace every instance of zoneId with a
 * new one that loads the files on disk now. warnSeconds: how long players are warned first. requesterId is the
 * character who asked (told how each move goes), 0 for the dashboard.
 */
struct WorldReloadRequest : public LUBitStream {
	WorldReloadRequest() : LUBitStream(ServiceType::MASTER, MessageType::Master::WORLD_RELOAD) {}

	static constexpr uint16_t DEFAULT_WARN_SECONDS = 10;

	uint32_t zoneId{};
	uint16_t warnSeconds{ DEFAULT_WARN_SECONDS };
	LWOOBJID requesterId{};
	std::string requestedBy;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write(zoneId);
		stream.Write(std::min(warnSeconds, InstanceMigrationRequest::MAX_WARN_SECONDS));
		stream.Write(requesterId);
		InstanceMigration::WriteText(stream, requestedBy, WorldFiles::MAX_BY);
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		if (!stream.Read(zoneId) || !stream.Read(warnSeconds) || !stream.Read(requesterId)) return false;
		if (warnSeconds > InstanceMigrationRequest::MAX_WARN_SECONDS) return false;
		return InstanceMigration::ReadText(stream, requestedBy, WorldFiles::MAX_BY);
	}
};

/**
 * WORLD_FILES_STATUS (master -> dashboard): every zone that runs, the files its instances loaded as they are on disk
 * now, and which instances loaded an older version. Sent when something changes and when the dashboard connects.
 */
struct WorldFilesStatus : public LUBitStream {
	WorldFilesStatus() : LUBitStream(ServiceType::MASTER, MessageType::Master::WORLD_FILES_STATUS) {}

	static constexpr uint16_t MAX_ZONES = 1024;
	static constexpr uint16_t MAX_INSTANCES = 1024;

	struct File {
		ZoneFileLog::Entry disk;   // as master last read it (packed: as the world reported it)
		bool hashed{};             // master has read it (not yet: disk is what a world reported)
		bool missing{};            // gone from disk
		bool changed{};            // an instance of the zone loaded another version

		bool operator==(const File& other) const = default;
	};

	struct Instance {
		uint32_t instanceId{};
		uint32_t cloneId{};
		int32_t players{};
		bool stale{};              // loaded a version of a file that isn't on disk any more
		bool reloading{};          // being replaced, or shutting down

		bool operator==(const Instance& other) const = default;
	};

	struct Zone {
		uint32_t zoneId{};
		std::vector<File> files;
		std::vector<Instance> instances;
		std::string message;       // the last reload of the zone

		bool operator==(const Zone& other) const = default;
	};

	bool watching{};
	uint16_t watchSeconds{};
	bool seamless{};
	std::vector<Zone> zones;

	void Serialize(RakNet::BitStream& stream) const override {
		stream.Write<uint8_t>(watching ? 1 : 0);
		stream.Write(watchSeconds);
		stream.Write<uint8_t>(seamless ? 1 : 0);
		const auto zoneCount = static_cast<uint16_t>(std::min<size_t>(zones.size(), MAX_ZONES));
		stream.Write(zoneCount);
		for (uint16_t z = 0; z < zoneCount; z++) {
			const auto& zone = zones[z];
			stream.Write(zone.zoneId);
			const auto fileCount = static_cast<uint16_t>(std::min<size_t>(zone.files.size(), WorldFiles::MAX_FILES));
			stream.Write(fileCount);
			for (uint16_t f = 0; f < fileCount; f++) {
				const auto& file = zone.files[f];
				WorldFiles::WriteFile(stream, file.disk);
				stream.Write<uint8_t>((file.hashed ? 1 : 0) | (file.missing ? 2 : 0) | (file.changed ? 4 : 0));
			}
			const auto instanceCount = static_cast<uint16_t>(std::min<size_t>(zone.instances.size(), MAX_INSTANCES));
			stream.Write(instanceCount);
			for (uint16_t i = 0; i < instanceCount; i++) {
				const auto& instance = zone.instances[i];
				stream.Write(instance.instanceId);
				stream.Write(instance.cloneId);
				stream.Write(instance.players);
				stream.Write<uint8_t>((instance.stale ? 1 : 0) | (instance.reloading ? 2 : 0));
			}
			InstanceMigration::WriteText(stream, zone.message, WorldFiles::MAX_MESSAGE);
		}
	}

	bool Deserialize(RakNet::BitStream& stream) override {
		uint8_t watchingValue{}, seamlessValue{};
		uint16_t zoneCount{};
		if (!stream.Read(watchingValue) || !stream.Read(watchSeconds) || !stream.Read(seamlessValue) || !stream.Read(zoneCount)) return false;
		if (zoneCount > MAX_ZONES) return false;
		watching = watchingValue != 0;
		seamless = seamlessValue != 0;
		zones.assign(zoneCount, {});
		for (auto& zone : zones) {
			uint16_t fileCount{};
			if (!stream.Read(zone.zoneId) || !stream.Read(fileCount) || fileCount > WorldFiles::MAX_FILES) return false;
			zone.files.assign(fileCount, {});
			for (auto& file : zone.files) {
				uint8_t flags{};
				if (!WorldFiles::ReadFile(stream, file.disk) || !stream.Read(flags)) return false;
				file.hashed = flags & 1;
				file.missing = flags & 2;
				file.changed = flags & 4;
			}
			uint16_t instanceCount{};
			if (!stream.Read(instanceCount) || instanceCount > MAX_INSTANCES) return false;
			zone.instances.assign(instanceCount, {});
			for (auto& instance : zone.instances) {
				uint8_t flags{};
				if (!stream.Read(instance.instanceId) || !stream.Read(instance.cloneId) || !stream.Read(instance.players) || !stream.Read(flags)) return false;
				instance.stale = flags & 1;
				instance.reloading = flags & 2;
			}
			if (!InstanceMigration::ReadText(stream, zone.message, WorldFiles::MAX_MESSAGE)) return false;
		}
		return true;
	}
};

#endif  //!__WORLDFILES__H__
