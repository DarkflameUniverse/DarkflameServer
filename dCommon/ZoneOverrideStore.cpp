#include "ZoneOverrideStore.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>

#include "BinaryPathFinder.h"
#include "ZonePublish.h"
#include "json.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
	using namespace ZoneOverrideStore;

	[[noreturn]] void Fail(const std::string& what) {
		throw StoreError(what);
	}

	// Flush a file's (or, where the system lets a folder be opened, a folder's) contents to disk
	void Sync(const fs::path& path) {
#ifdef _WIN32
		FILE* file = nullptr;
		if (fopen_s(&file, path.string().c_str(), "rb+") != 0 || !file) return; // folders can't be opened this way
		fflush(file);
		_commit(_fileno(file));
		fclose(file);
#else
		const int fd = ::open(path.c_str(), O_RDONLY);
		if (fd < 0) Fail("Could not open " + path.string() + " to flush it");
		const int result = ::fsync(fd);
		::close(fd);
		if (result != 0) Fail("Could not flush " + path.string());
#endif
	}

	void SyncTree(const fs::path& folder) {
		std::error_code code;
		for (fs::recursive_directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
			if (it->is_regular_file(code) || it->is_directory(code)) Sync(it->path());
		}
		if (code) Fail("Could not read " + folder.string());
		Sync(folder);
	}

	void MakeDirs(const fs::path& folder) {
		std::error_code code;
		fs::create_directories(folder, code);
		if (code) Fail("Could not make " + folder.string());
	}

	// The same file under another name where the file system can (sets are never changed in place), else a copy
	void LinkOrCopy(const fs::path& from, const fs::path& to) {
		MakeDirs(to.parent_path());
		std::error_code code;
		fs::create_hard_link(from, to, code);
		if (!code) return;
		fs::copy_file(from, to, fs::copy_options::overwrite_existing, code);
		if (code) Fail("Could not copy " + from.string());
	}

	void WriteText(const fs::path& path, const std::string& text) {
		MakeDirs(path.parent_path());
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out << text;
		out.flush();
		if (!out) Fail("Could not write " + path.string());
	}

	std::optional<json> ReadJson(const fs::path& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return std::nullopt;
		auto parsed = json::parse(in, nullptr, false);
		if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
		return parsed;
	}

	json ToJson(const Entry& entry) {
		return { {"path", entry.path}, {"bytes", entry.bytes}, {"sha256", entry.sha256}, {"version", entry.version}, {"who", entry.who}, {"when", entry.when} };
	}

	std::vector<Entry> EntriesOf(const json& manifest) {
		std::vector<Entry> entries;
		const auto files = manifest.find("files");
		if (files == manifest.end() || !files->is_array()) return entries;
		for (const auto& file : *files) {
			if (!file.is_object()) continue;
			Entry entry;
			entry.path = file.value("path", std::string());
			entry.bytes = file.value("bytes", uint64_t{});
			entry.sha256 = file.value("sha256", std::string());
			entry.version = file.value("version", uint32_t{});
			entry.who = file.value("who", std::string());
			entry.when = file.value("when", int64_t{});
			// A manifest is only ever written here, but it is read back from disk: its paths are checked again
			if (ZonePublish::NameProblem(entry.path) || ZonePublish::Canonical(entry.path) != entry.path) continue;
			entries.push_back(std::move(entry));
		}
		return entries;
	}

	std::string ManifestName(uint32_t zoneId) {
		return ".zone-" + std::to_string(zoneId) + ".json";
	}

	struct Folders {
		fs::path root;      // resolved
		fs::path maps;      // <root>/maps
		fs::path live;      // <root>/maps/<zone folder>
		fs::path staging;   // <root>/staging/<id>
		fs::path backups;   // <root>/backups/<zone id>
		std::string folder; // the zone folder, no trailing '/'
	};

	Folders Resolve(const fs::path& root, uint32_t zoneId, const std::string& zoneFolder, const std::string& id) {
		if (root.empty()) Fail("No override folder is set (zone_override_dir)");
		Folders folders;
		std::error_code code;
		fs::create_directories(root / "maps", code);
		folders.root = fs::weakly_canonical(root, code);
		if (code) Fail("The override folder can't be used: " + root.string());
		folders.maps = folders.root / "maps";
		folders.folder = zoneFolder;
		while (!folders.folder.empty() && folders.folder.back() == '/') folders.folder.pop_back();
		if (folders.folder.empty()) Fail("The zone's files are not in a folder of their own");
		const auto live = ZonePublish::ResolveInside(folders.maps, folders.folder);
		if (!live) Fail("The zone's folder is not inside the override folder");
		folders.live = *live;
		if (!id.empty()) {
			if (!ValidId(id)) Fail("Not a usable id");
			folders.staging = folders.root / "staging" / id;
		}
		folders.backups = folders.root / "backups" / std::to_string(zoneId);
		return folders;
	}

	// Swap two folders that both exist, in one step where the system has one
	void SwapFolders(const fs::path& incoming, const fs::path& live, const fs::path& aside) {
#ifdef __linux__
		if (::syscall(SYS_renameat2, AT_FDCWD, incoming.c_str(), AT_FDCWD, live.c_str(), RENAME_EXCHANGE) == 0) return;
#endif
		std::error_code code;
		fs::rename(live, aside, code);
		if (code) Fail("Could not move the zone's folder aside");
		fs::rename(incoming, live, code);
		if (code) {
			std::error_code undo;
			fs::rename(aside, live, undo);
			Fail("Could not move the new files into place");
		}
	}

	std::vector<Entry> CurrentOf(const Folders& folders, uint32_t zoneId, json* manifestOut = nullptr) {
		const auto manifest = ReadJson(folders.live / ManifestName(zoneId));
		if (!manifest) return {};
		if (manifestOut) *manifestOut = *manifest;
		auto entries = EntriesOf(*manifest);
		const auto prefix = folders.folder + "/";
		std::erase_if(entries, [&](const Entry& entry) { return !entry.path.starts_with(prefix); });
		return entries;
	}

	void PruneBackups(const fs::path& backups, uint32_t keep) {
		std::vector<std::string> ids;
		std::error_code code;
		for (fs::directory_iterator it(backups, code), end; !code && it != end; it.increment(code)) {
			if (it->is_directory(code) && ValidId(it->path().filename().string())) ids.push_back(it->path().filename().string());
		}
		// "<unix time>-<id>": newest first by time, then by name
		const auto timeOf = [](const std::string& id) { return std::strtoll(id.c_str(), nullptr, 10); };
		std::ranges::sort(ids, [&](const std::string& a, const std::string& b) {
			return timeOf(a) != timeOf(b) ? timeOf(a) > timeOf(b) : a > b;
		});
		for (size_t i = keep; i < ids.size(); ++i) fs::remove_all(backups / ids[i], code);
	}

	// One change at a time in an override folder, whichever server makes it (the lock goes when its holder does)
	struct FolderLock {
		explicit FolderLock(const fs::path& root) {
#ifndef _WIN32
			m_File = ::open((root / ".lock").c_str(), O_CREAT | O_RDWR, 0644);
			if (m_File < 0 || ::flock(m_File, LOCK_EX) != 0) Fail("The override folder can't be locked");
#else
			(void)root;
#endif
		}
		~FolderLock() {
#ifndef _WIN32
			if (m_File >= 0) ::close(m_File);
#endif
		}
		FolderLock(const FolderLock&) = delete;
		FolderLock& operator=(const FolderLock&) = delete;
	private:
		int m_File = -1;
	};

	/**
	 * The one way a zone's set changes. `place` puts the new set's files under the folder it is given (laid out as
	 * under res/maps/) and returns their entries; everything else is the same for a new set, a restore and a clear.
	 */
	template<typename Place>
	Applied Replace(const fs::path& root, const Change& change, Place place) {
		const auto folders = Resolve(root, change.zoneId, change.zoneFolder, change.id);
		const FolderLock lock(folders.root);
		const auto incomingMaps = folders.staging / "incoming";
		const auto incoming = incomingMaps / folders.folder;
		std::error_code code;
		fs::remove_all(incomingMaps, code);
		MakeDirs(incoming);

		json oldManifest;
		const auto previous = CurrentOf(folders, change.zoneId, &oldManifest);
		std::set<std::string> previousPaths;
		for (const auto& entry : previous) previousPaths.insert(entry.path);

		// Everything in the folder that is not this zone's set stays (zones can share a folder)
		const bool hadFolder = fs::is_directory(folders.live, code);
		if (hadFolder) {
			for (fs::recursive_directory_iterator it(folders.live, code), end; !code && it != end; it.increment(code)) {
				if (!it->is_regular_file(code)) continue;
				const auto relative = folders.folder + "/" + it->path().lexically_relative(folders.live).generic_string();
				if (previousPaths.contains(relative) || it->path().filename() == ManifestName(change.zoneId)) continue;
				LinkOrCopy(it->path(), incomingMaps / relative);
			}
			if (code) Fail("Could not read the zone's folder");
		}

		auto entries = place(incomingMaps, folders);
		const auto prefix = folders.folder + "/";
		for (auto& entry : entries) {
			if (!entry.path.starts_with(prefix)) Fail("A file is not in the zone's folder: " + entry.path);
			const auto target = ZonePublish::ResolveInside(incomingMaps, entry.path);
			if (!target || !fs::is_regular_file(*target, code)) Fail("A file did not arrive where it belongs: " + entry.path);
		}
		if (!entries.empty()) {
			json files = json::array();
			for (const auto& entry : entries) files.push_back(ToJson(entry));
			WriteText(incoming / ManifestName(change.zoneId), json{ {"zoneId", change.zoneId}, {"id", change.id}, {"who", change.who},
				{"note", change.note}, {"time", change.now}, {"files", files} }.dump(1));
		}
		SyncTree(incomingMaps);

		// The set being replaced is kept first
		Applied applied;
		if (!previous.empty()) {
			const auto backupId = std::to_string(change.now) + "-" + change.id;
			const auto backup = folders.backups / backupId;
			fs::remove_all(backup, code);
			for (const auto& entry : previous) {
				const auto from = ZonePublish::ResolveInside(folders.maps, entry.path);
				if (!from || !fs::is_regular_file(*from, code)) continue;
				LinkOrCopy(*from, backup / "files" / entry.path);
			}
			oldManifest["replacedAt"] = change.now;
			WriteText(backup / "manifest.json", oldManifest.dump(1));
			SyncTree(backup);
			Sync(folders.backups);
			applied.backupId = backupId;
		}

		// The swap
		if (hadFolder) {
			SwapFolders(incoming, folders.live, folders.staging / "replaced");
		} else {
			MakeDirs(folders.live.parent_path());
			fs::rename(incoming, folders.live, code);
			if (code) Fail("Could not move the new files into place");
		}
		Sync(folders.live.parent_path());

		fs::remove_all(folders.staging, code);
		PruneBackups(folders.backups, change.keepBackups);
		applied.files = std::move(entries);
		return applied;
	}
}

namespace ZoneOverrideStore {
	std::filesystem::path RootFromSetting(const std::string& value) {
		if (value.empty()) return {};
		fs::path root(value);
		if (root.is_relative()) root = BinaryPathFinder::GetBinaryDir() / root;
		return root.lexically_normal();
	}

	bool ValidId(std::string_view text) {
		if (text.empty() || text.size() > 64) return false;
		return std::ranges::all_of(text, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
	}

	std::filesystem::path MakeUploadDir(const fs::path& root, const std::string& id) {
		if (root.empty() || !ValidId(id)) Fail("No override folder is set (zone_override_dir)");
		std::error_code code;
		fs::create_directories(root, code);
		const auto resolved = fs::weakly_canonical(root, code);
		if (code) Fail("The override folder can't be used: " + root.string());
		const auto folder = resolved / "staging" / id / "upload";
		fs::remove_all(folder, code);
		MakeDirs(folder);
		return folder;
	}

	void RemoveStaging(const fs::path& root, const std::string& id) {
		if (root.empty() || !ValidId(id)) return;
		std::error_code code;
		fs::remove_all(root / "staging" / id, code);
	}

	void RemoveAllStaging(const fs::path& root) {
		if (root.empty()) return;
		std::error_code code;
		fs::remove_all(root / "staging", code);
	}

	Applied Apply(const fs::path& root, const Change& change, const std::vector<NewFile>& files) {
		if (files.empty()) Fail("A set has at least the zone's .luz");
		return Replace(root, change, [&](const fs::path& incomingMaps, const Folders&) {
			std::vector<Entry> entries;
			std::set<std::string> seen;
			for (const auto& file : files) {
				if (ZonePublish::Canonical(file.path) != file.path || !seen.insert(file.path).second) Fail("Not a usable file name: " + file.path);
				const auto target = ZonePublish::ResolveInside(incomingMaps, file.path);
				if (!target) Fail("Not a usable file name: " + file.path);
				// Linked, never moved: a file may be one the zone has in use already, carried over into the new set
				LinkOrCopy(file.source, *target);
				entries.push_back({ file.path, file.bytes, file.sha256, file.version, change.who, change.now });
			}
			return entries;
		});
	}

	Applied Restore(const fs::path& root, const Change& change, const std::string& backupId) {
		if (!ValidId(backupId)) Fail("No such backup");
		return Replace(root, change, [&](const fs::path& incomingMaps, const Folders& folders) {
			const auto backup = folders.backups / backupId;
			const auto manifest = ReadJson(backup / "manifest.json");
			if (!manifest) Fail("No such backup");
			auto entries = EntriesOf(*manifest);
			if (entries.empty()) Fail("The backup holds no files");
			for (const auto& entry : entries) {
				const auto from = ZonePublish::ResolveInside(backup / "files", entry.path);
				const auto to = ZonePublish::ResolveInside(incomingMaps, entry.path);
				std::error_code code;
				if (!from || !to || !fs::is_regular_file(*from, code)) Fail("The backup is missing " + entry.path);
				if (ZonePublish::Sha256File(*from) != std::optional<std::string>(entry.sha256)) Fail("The backup's " + entry.path + " is not the file that was kept");
				LinkOrCopy(*from, *to);
			}
			return entries;
		});
	}

	Applied Clear(const fs::path& root, const Change& change) {
		return Replace(root, change, [](const fs::path&, const Folders&) { return std::vector<Entry>{}; });
	}

	std::vector<Entry> Current(const fs::path& root, uint32_t zoneId, const std::string& zoneFolder) {
		if (root.empty()) return {};
		try {
			return CurrentOf(Resolve(root, zoneId, zoneFolder, ""), zoneId);
		} catch (const StoreError&) {
			return {};
		}
	}

	std::vector<Backup> Backups(const fs::path& root, uint32_t zoneId) {
		std::vector<Backup> backups;
		if (root.empty()) return backups;
		const auto folder = root / "backups" / std::to_string(zoneId);
		std::error_code code;
		for (fs::directory_iterator it(folder, code), end; !code && it != end; it.increment(code)) {
			const auto id = it->path().filename().string();
			if (!it->is_directory(code) || !ValidId(id)) continue;
			const auto manifest = ReadJson(it->path() / "manifest.json");
			if (!manifest) continue;
			Backup backup;
			backup.id = id;
			backup.time = manifest->value("replacedAt", int64_t{});
			backup.who = manifest->value("who", std::string());
			backup.note = manifest->value("note", std::string());
			backup.files = EntriesOf(*manifest);
			backups.push_back(std::move(backup));
		}
		std::ranges::sort(backups, [](const Backup& a, const Backup& b) { return a.time != b.time ? a.time > b.time : a.id > b.id; });
		return backups;
	}
}
