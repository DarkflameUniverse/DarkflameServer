#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * The override folder (zone_override_dir, docs/ZonePublish.md): the edited zone files the servers read in place of
 * the client's (AssetManager::SetOverrideDir), kept apart from the client's files, which are never written.
 *
 *   <root>/maps/<zone folder>/...                  the files in use, where the asset manager looks
 *   <root>/maps/<zone folder>/.zone-<id>.json      which of them are zone <id>'s set, who sent it and when
 *   <root>/staging/<id>/                           a set while it is checked and built
 *   <root>/backups/<zone id>/<time>-<id>/          the sets a zone had before, newest kept
 *
 * A zone's set is replaced as a whole: the new contents of its folder are built in staging, flushed to disk, and
 * swapped in with a rename; the set it replaces goes to backups first. Restoring a backup and clearing a zone's set
 * are the same swap. Knows nothing of who may do this (ZoneEditLock) or how files arrive.
 *
 * Only reads and writes files under root. Changes in one override folder happen one at a time, whichever server
 * makes them (a file lock, where the system has one). No settings, database or logging, so it can run on a worker
 * thread.
 */
namespace ZoneOverrideStore {
	struct StoreError : std::runtime_error {
		using std::runtime_error::runtime_error;
	};

	struct Entry {
		std::string path;    // relative to res/maps/
		uint64_t bytes{};
		std::string sha256;
		uint32_t version{};
		std::string who;
		int64_t when{};
	};

	struct Backup {
		std::string id;      // "<unix time>-<set id>"
		int64_t time{};      // when the set was replaced
		std::string who;     // who sent the set it holds
		std::string note;
		std::vector<Entry> files;
	};

	// A file of a new set: where its bytes are now (linked or copied into the store; the source stays)
	struct NewFile {
		std::string path;
		std::filesystem::path source;
		uint64_t bytes{};
		std::string sha256;
		uint32_t version{};
	};

	struct Applied {
		std::optional<std::string> backupId; // the set that was replaced, when there was one
		std::vector<Entry> files;            // the zone's set now
	};

	struct Change {
		uint32_t zoneId{};
		std::string zoneFolder;  // the zone's .luz folder relative to res/maps/, with its trailing '/'
		std::string id;          // unique per change: letters, digits and '-' only
		std::string who;
		std::string note;
		int64_t now{};           // unix seconds
		uint32_t keepBackups = 20;
	};

	// The folder a zone_override_dir value names: as written when absolute, else under the server's own folder.
	// Empty for an empty value (the feature is off).
	std::filesystem::path RootFromSetting(const std::string& value);

	// Whether text can be a set or backup id: 1 to 64 of a-z, 0-9 and '-'
	bool ValidId(std::string_view text);

	// Where a transport may put a set's files while it is checked: <root>/staging/<id>/upload (made empty)
	std::filesystem::path MakeUploadDir(const std::filesystem::path& root, const std::string& id);
	// Removes everything of <root>/staging/<id>
	void RemoveStaging(const std::filesystem::path& root, const std::string& id);
	// Removes every staging folder (left behind when a server stopped mid-way); call before any set is taken
	void RemoveAllStaging(const std::filesystem::path& root);

	// Replace the zone's set with these files. Throws StoreError with nothing changed when it can't.
	Applied Apply(const std::filesystem::path& root, const Change& change, const std::vector<NewFile>& files);
	// Replace the zone's set with a backup's. Throws StoreError (also for an unknown backup).
	Applied Restore(const std::filesystem::path& root, const Change& change, const std::string& backupId);
	// Take the zone's set away, so the client's files are read again
	Applied Clear(const std::filesystem::path& root, const Change& change);

	std::vector<Entry> Current(const std::filesystem::path& root, uint32_t zoneId, const std::string& zoneFolder);
	// Newest first
	std::vector<Backup> Backups(const std::filesystem::path& root, uint32_t zoneId);
}
