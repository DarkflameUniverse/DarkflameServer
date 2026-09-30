#ifndef __WORLDFILEWATCH__H__
#define __WORLDFILEWATCH__H__

#include <compare>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "FdbSnapshot.h"
#include "ZoneFileLog.h"
#include "master/InstanceMigration.h"

/**
 * World hot reload without master's state (docs/WorldHotReload.md): which zone files the running instances loaded,
 * which of those changed on disk, and which instances to replace. Header only, so it is unit tested without a master.
 *
 * Master reads (hashes) each watched file itself: once when a world first reports it, and again whenever its size or
 * mtime changed and then held still for one poll (FdbSnapshot::Watcher). An instance is stale when a file it loaded
 * has another hash on disk now. Files a world read from the client's packs are listed but not watched.
 */
namespace WorldFileWatch {
	using Stamp = FdbSnapshot::Stamp;

	struct InstanceKey {
		uint32_t zone{};
		uint32_t instance{};

		auto operator<=>(const InstanceKey&) const = default;
	};

	struct FileState {
		ZoneFileLog::eKind kind{};
		bool packed{};
		uint64_t reportedSize{};            // what the first world to report it read
		uint64_t reportedHash{};
		std::optional<uint64_t> hash;       // master's read of the file on disk
		uint64_t size{};
		bool missing{};                     // the last poll or read found no file
		bool hashing{};
		FdbSnapshot::Watcher watcher;
	};

	struct Loaded {
		uint32_t clone{};
		std::map<std::string, uint64_t> files; // path -> hash the world read
	};

	// A file to hash now, with the stamp it had when the poll saw it (accepted once the hash is in)
	struct HashJob {
		std::string path;
		Stamp stamp;
	};

	class Tracker {
	public:
		// A world reported the files it loaded (replaces what that instance reported before)
		void Report(uint32_t zone, uint32_t instance, uint32_t clone, const std::vector<ZoneFileLog::Entry>& files) {
			auto& loaded = m_Instances[{ zone, instance }];
			loaded = {};
			loaded.clone = clone;
			for (const auto& file : files) {
				loaded.files[file.path] = file.hash;
				auto [it, added] = m_Files.try_emplace(file.path);
				if (!added) continue;
				it->second.kind = file.kind;
				it->second.packed = file.packed;
				it->second.reportedSize = file.size;
				it->second.reportedHash = file.hash;
			}
		}

		// An instance stopped; files no instance loaded any more stop being watched
		void Forget(uint32_t zone, uint32_t instance) {
			m_Instances.erase({ zone, instance });
			std::erase_if(m_Files, [this](const auto& entry) { return !entry.second.hashing && !IsLoaded(entry.first); });
		}

		/**
		 * Looks at every watched file (stampOf: its size and mtime now). Returns the ones to hash: files master hasn't
		 * read yet, and files whose size or mtime changed and held still since the last poll. Those are marked as
		 * being hashed until Hashed is called for them.
		 */
		std::vector<HashJob> Poll(const std::function<Stamp(const std::string&)>& stampOf) {
			std::vector<HashJob> due;
			for (auto& [path, file] : m_Files) {
				if (file.packed || file.hashing) continue;
				const auto stamp = stampOf(path);
				file.missing = !stamp.exists;
				const bool first = !file.hash && stamp.exists;
				if (!file.watcher.Poll(stamp) && !first) continue;
				file.hashing = true;
				due.push_back({ path, stamp });
			}
			return due;
		}

		/**
		 * A hash finished (hash nullopt: the file couldn't be read). Returns true when the file on disk is another
		 * version than master read before.
		 */
		bool Hashed(const HashJob& job, std::optional<uint64_t> hash, uint64_t size) {
			const auto it = m_Files.find(job.path);
			if (it == m_Files.end()) return false;
			auto& file = it->second;
			file.hashing = false;
			// Taken either way, so an unreadable file isn't read again every poll; the next change to it is
			file.watcher.Accept(job.stamp);
			if (!hash) {
				file.missing = true;
				if (!IsLoaded(job.path)) m_Files.erase(it);
				return false;
			}
			file.missing = false;
			const bool changed = file.hash && *file.hash != *hash;
			file.hash = hash;
			file.size = size;
			if (!IsLoaded(job.path)) m_Files.erase(it);
			return changed;
		}

		// A file this instance loaded is another version on disk now
		bool IsStale(const InstanceKey& key) const {
			const auto it = m_Instances.find(key);
			if (it == m_Instances.end()) return false;
			for (const auto& [path, loadedHash] : it->second.files) {
				const auto file = m_Files.find(path);
				if (file != m_Files.end() && file->second.hash && *file->second.hash != loadedHash) return true;
			}
			return false;
		}

		std::vector<InstanceKey> Stale() const {
			std::vector<InstanceKey> stale;
			for (const auto& [key, loaded] : m_Instances) {
				if (IsStale(key)) stale.push_back(key);
			}
			return stale;
		}

		// The versions on disk of the files this instance loaded; an automatic reload is tried once per signature
		uint64_t Signature(const InstanceKey& key) const {
			uint64_t signature = 14695981039346656037ULL;
			const auto it = m_Instances.find(key);
			if (it == m_Instances.end()) return signature;
			for (const auto& [path, loadedHash] : it->second.files) {
				const auto file = m_Files.find(path);
				const uint64_t onDisk = file != m_Files.end() && file->second.hash ? *file->second.hash : loadedHash;
				signature = FdbSnapshot::Hash(reinterpret_cast<const uint8_t*>(path.data()), path.size(), signature);
				signature = FdbSnapshot::Hash(reinterpret_cast<const uint8_t*>(&onDisk), sizeof(onDisk), signature);
			}
			return signature;
		}

		// The files of a stale instance that changed (for the log)
		std::vector<std::string> ChangedFiles(const InstanceKey& key) const {
			std::vector<std::string> changed;
			const auto it = m_Instances.find(key);
			if (it == m_Instances.end()) return changed;
			for (const auto& [path, loadedHash] : it->second.files) {
				const auto file = m_Files.find(path);
				if (file != m_Files.end() && file->second.hash && *file->second.hash != loadedHash) changed.push_back(path);
			}
			return changed;
		}

		// Any instance of zone loaded another version of path than is on disk
		bool IsChanged(uint32_t zone, const std::string& path) const {
			const auto file = m_Files.find(path);
			if (file == m_Files.end() || !file->second.hash) return false;
			for (const auto& [key, loaded] : m_Instances) {
				if (key.zone != zone) continue;
				const auto it = loaded.files.find(path);
				if (it != loaded.files.end() && it->second != *file->second.hash) return true;
			}
			return false;
		}

		// Every file some instance of zone loaded
		std::set<std::string> FilesOf(uint32_t zone) const {
			std::set<std::string> files;
			for (const auto& [key, loaded] : m_Instances) {
				if (key.zone != zone) continue;
				for (const auto& [path, hash] : loaded.files) files.insert(path);
			}
			return files;
		}

		std::set<uint32_t> Zones() const {
			std::set<uint32_t> zones;
			for (const auto& [key, loaded] : m_Instances) zones.insert(key.zone);
			return zones;
		}

		bool Knows(const InstanceKey& key) const { return m_Instances.contains(key); }
		const std::map<std::string, FileState>& Files() const { return m_Files; }

	private:
		bool IsLoaded(const std::string& path) const {
			for (const auto& [key, loaded] : m_Instances) {
				if (loaded.files.contains(path)) return true;
			}
			return false;
		}

		std::map<std::string, FileState> m_Files;
		std::map<InstanceKey, Loaded> m_Instances;
	};

	enum class eAction : uint8_t {
		REPLACE,          // start a new instance (same clone, same password), move its players there, stop it
		START_THEN_STOP,  // nobody there, but the zone always has an instance: start a new one, stop this one
		STOP,             // nobody there: stop it (a new instance starts when someone goes there)
		SKIP,
		// A property with players: never moved (building in progress isn't saved). It takes nobody new, its players
		// are told an update is waiting, and it stops once everyone left (OutdatedInstances.h)
		KEEP_UNTIL_EMPTY,
	};

	inline const char* ActionName(eAction action) {
		switch (action) {
		case eAction::REPLACE: return "replace";
		case eAction::START_THEN_STOP: return "start a new one, then stop it";
		case eAction::STOP: return "stop";
		case eAction::SKIP: return "skip";
		case eAction::KEEP_UNTIL_EMPTY: return "keep until everyone left";
		}
		return "skip";
	}

	struct Choice {
		InstanceMigration::InstanceView view;
		eAction action{ eAction::SKIP };
		std::string reason;
	};

	/**
	 * What to do with each instance wanted() picks: replace it when players are there (a property is kept until they
	 * all left instead), stop it when it is empty (a zone in keepZones gets one new public instance first). Character selection loads no zone; instances still starting
	 * load the files on disk now (and report them); instances shutting down or being emptied already are left alone.
	 */
	inline std::vector<Choice> Choose(const std::vector<InstanceMigration::InstanceView>& instances,
		const std::function<bool(const InstanceMigration::InstanceView&)>& wanted, const std::set<uint32_t>& keepZones) {
		std::vector<Choice> choices;
		for (const auto& view : instances) {
			if (!wanted(view)) continue;
			Choice choice;
			choice.view = view;
			if (view.zoneId == 0) choice.reason = "Character selection loads no zone";
			else if (view.shuttingDown) choice.reason = "Already shutting down";
			else if (view.draining) choice.reason = "Its players are already being moved";
			else if (!view.ready) choice.reason = "Still starting; it loads the files on disk now";
			else if (view.players > 0) choice.action = view.cloneId != 0 ? eAction::KEEP_UNTIL_EMPTY : eAction::REPLACE;
			else choice.action = eAction::STOP;
			choices.push_back(std::move(choice));
		}
		// A zone that always has an instance keeps one: a public instance replaced for its players, or else one new
		// instance started for the first empty public one
		std::set<uint32_t> keptZones;
		for (const auto& choice : choices) {
			if (choice.action == eAction::REPLACE && !choice.view.isPrivate && choice.view.cloneId == 0) keptZones.insert(choice.view.zoneId);
		}
		for (auto& choice : choices) {
			const auto& view = choice.view;
			if (choice.action != eAction::STOP || !keepZones.contains(view.zoneId) || view.isPrivate || view.cloneId != 0 || keptZones.contains(view.zoneId)) continue;
			choice.action = eAction::START_THEN_STOP;
			keptZones.insert(view.zoneId);
		}
		return choices;
	}
}

#endif  //!__WORLDFILEWATCH__H__
