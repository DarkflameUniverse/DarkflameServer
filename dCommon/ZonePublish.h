#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/**
 * Publishing edited zone files (docs/ZonePublish.md): checking a set of files an external level editor sent for one
 * zone before any of it is kept. Nothing here knows how the files arrived (the dashboard's API, or any other way in)
 * or who sent them; it reads the files it is given and says, file by file, what is wrong with them. Applying a set
 * that passed is ZoneOverrideStore's, who may send one is ZoneEditLock's.
 *
 * Reads files and nothing else (no settings, no database, no logging of its own), so it can run on a worker thread.
 */
namespace ZonePublish {
	constexpr size_t MAX_NAME_LENGTH = 200;
	// Errors listed per file; a file with more is cut off there
	constexpr size_t MAX_ERRORS_PER_FILE = 20;

	struct Limits {
		uint64_t maxFileBytes = 64ULL * 1024 * 1024;
		uint64_t maxTotalBytes = 256ULL * 1024 * 1024;
		uint32_t maxFiles = 128;
		uint32_t maxObjects = 100000; // in every scene of the set together
	};

	// The zone a set is for, as the server knows it
	struct Zone {
		uint32_t zoneId{};
		// The zone's .luz from the ZoneTable, relative to res/maps/, lowercase with forward slashes
		std::string luzPath;
		// The scene files the zone has in the server's client files, as luzPath is written; a set can't add scenes
		std::set<std::string> clientScenes;
		// Whether a file the set leaves out is there for the zone already (an earlier set's, or the client's)
		std::function<bool(const std::string& path)> hasFile;
	};

	// One file of a set: its name as sent, and where its bytes are
	struct Upload {
		std::string name;
		std::filesystem::path file;
		uint64_t bytes{};
	};

	struct Error {
		std::string code;
		std::string message;
		std::optional<uint64_t> offset;
	};

	struct File {
		std::string path;    // as sent when it was refused by name, else relative to res/maps/ as it is kept
		bool ok = true;
		std::vector<Error> errors;
		uint64_t bytes{};
		std::string sha256;
		uint32_t version{};  // the file's own format version (0 for triggers)
		std::filesystem::path source; // the upload it came from
	};

	struct Result {
		bool ok = true;
		std::vector<File> files; // every file sent, in the order sent; then a missing .luz
		uint32_t objects{};
	};

	// A zone's folder: its .luz path up to and including the last '/'
	std::string FolderOf(std::string_view luzPath);

	// Lowercase with forward slashes kept: how a path is compared and kept. ASCII only (other bytes never pass NameProblem).
	std::string Canonical(std::string_view name);

	/**
	 * What is wrong with a file name as sent, by its spelling alone; nullopt when nothing is. A name is a path relative
	 * to res/maps/: no empty, "." or ".." part, no leading '/', no backslash, drive or NUL, printable ASCII only, and
	 * no part that starts or ends with a space or ends with a dot.
	 */
	std::optional<std::string> NameProblem(std::string_view name);

	/**
	 * The path a file of this zone is written to under root, when that path is inside root once every link and
	 * ".." on the way is resolved (std::filesystem::weakly_canonical). nullopt otherwise.
	 */
	std::optional<std::filesystem::path> ResolveInside(const std::filesystem::path& root, const std::string& relative);

	/**
	 * Checks a whole set. Every file is read with the server's own readers; a set is good only when every file is
	 * (Result::ok), and nothing is written here either way.
	 */
	Result Validate(const Zone& zone, const std::vector<Upload>& uploads, const Limits& limits);

	// What a .lutriggers file has wrong with it (parse_failed, unknown_trigger_event, unknown_trigger_command,
	// trigger_missing_target)
	std::vector<Error> CheckTriggers(std::string_view xml);

	// One file's own checks, picked by what its name ends in (.luz, .lvl, .lutriggers, .raw): whether it reads to its
	// end in a version the server takes. What a .luz names is Validate's to check.
	std::vector<Error> CheckContents(std::string_view name, const std::string& data, uint32_t* version = nullptr);

	// SHA-256 of a file as lowercase hex; nullopt when it can't be read
	std::optional<std::string> Sha256File(const std::filesystem::path& file);
	std::string Sha256(std::string_view bytes);
}
