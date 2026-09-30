#ifndef FDBSNAPSHOT_H
#define FDBSNAPSHOT_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

/**
 * Content-addressed copies of the client's cdclient.fdb, so no server maps the client's own file.
 *
 * Master copies <res>/cdclient.fdb to resServer/cdclient-<hash>.fdb (the hash is of the copy's bytes) and makes the
 * matching resServer/CDServer-<hash>.sqlite from it. A small pointer file, resServer/cdclient-current, names the pair
 * every server should use. A new version of the fdb gets new names, so nothing is ever replaced while a process has
 * it open (Windows can't rename over or delete an open file; POSIX keeps the old one alive until it is closed).
 *
 * Nothing here logs or touches shared state, so it is safe on a worker thread.
 */
namespace FdbSnapshot {
	// The pointer file in resServer
	constexpr const char* CURRENT_FILE = "cdclient-current";
	// The SQLite file the server has always used, when no copy has its own yet
	constexpr const char* DEFAULT_SQLITE = "CDServer.sqlite";

	// 64-bit FNV-1a of the bytes
	uint64_t Hash(const uint8_t* data, uint64_t size, uint64_t seed = 14695981039346656037ULL);

	// Hash of a whole file, nullopt if it can't be read
	std::optional<uint64_t> HashFile(const std::filesystem::path& path);

	// 16 lowercase hex digits
	std::string HashText(uint64_t hash);

	// cdclient-<hash>.fdb
	std::string FdbName(uint64_t hash);

	// CDServer-<hash>.sqlite
	std::string SqliteName(uint64_t hash);

	// The hash in a cdclient-<hash>.fdb or CDServer-<hash>.sqlite name
	std::optional<uint64_t> ParseName(const std::string& name);

	/**
	 * Copies source into dir as cdclient-<hash>.fdb, the hash taken of the copy so the name matches its bytes even if
	 * source changes during the copy. The copy is written under a temporary name and renamed into place; if a copy
	 * with that hash is already there it is kept and the temporary one removed.
	 *
	 * @return the hash, or nullopt with error set
	 */
	std::optional<uint64_t> MakeCopy(const std::filesystem::path& source, const std::filesystem::path& dir, std::string& error);

	// The pair of files every server should use
	struct Current {
		std::string fdb;
		std::string sqlite;

		bool operator==(const Current& other) const { return fdb == other.fdb && sqlite == other.sqlite; }
	};

	std::optional<Current> ReadCurrent(const std::filesystem::path& dir);

	// Writes the pointer file under a temporary name and renames it over the old one (it is never held open)
	bool WriteCurrent(const std::filesystem::path& dir, const Current& current);

	/**
	 * Removes every cdclient-<hash>.fdb and CDServer-<hash>.sqlite in dir whose hash is not in keep, and any leftover
	 * temporary copies. A file that can't be removed (Windows refuses while a process still maps it) is left for the
	 * next call. CDServer.sqlite and the client's own fdb are never touched.
	 *
	 * @return the names removed
	 */
	std::vector<std::string> RemoveOld(const std::filesystem::path& dir, const std::set<uint64_t>& keep);

	// What a poll of the client's fdb saw
	struct Stamp {
		uint64_t size = 0;
		int64_t mtime = 0;
		bool exists = false;

		bool operator==(const Stamp& other) const { return size == other.size && mtime == other.mtime && exists == other.exists; }
		bool operator!=(const Stamp& other) const { return !(*this == other); }
	};

	Stamp StampOf(const std::filesystem::path& path);

	/**
	 * Decides when a changed file is worth hashing: once its size and mtime differ from the version last taken and
	 * have stayed the same for one poll, so a file still being written isn't copied halfway.
	 */
	class Watcher {
	public:
		// The version that is current now (taken at startup or after a reload)
		void Accept(const Stamp& stamp) { m_Accepted = stamp; m_Last = stamp; }

		// true when the file should be hashed now
		bool Poll(const Stamp& stamp);

	private:
		Stamp m_Accepted;
		Stamp m_Last;
	};

	// Rows and a content hash per table of an fdb, to log what a new version changed
	struct TableSummary {
		uint64_t rows = 0;
		uint64_t hash = 0;
	};

	std::map<std::string, TableSummary> Summarize(const std::filesystem::path& fdb);

	// One line per table that was added, removed or changed ("Objects: 16012 -> 16015 rows")
	std::vector<std::string> DescribeChanges(const std::map<std::string, TableSummary>& before, const std::map<std::string, TableSummary>& after);
};

#endif // FDBSNAPSHOT_H
