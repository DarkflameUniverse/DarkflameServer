#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

/**
 * Log bundles: the servers' log files (and crash dumps) picked by date range, server and world, put in one zip file to
 * download from the System Log page or GET /api/logs/bundle. Only reads files, so it runs on the dashboard's worker
 * threads; the routes (ServerRoutes.cpp) read the settings on the web thread and pass the folders in.
 */
namespace LogBundle {
	// What a log file's name says. Log files are <Server>_<start time>.log, world servers'
	// WorldServer_<zone>_<clone>_<instance>_<start time>.log; crash dumps are Crash_<process name>_<pid>.log.
	struct LogName {
		std::string server; // MasterServer, AuthServer, ChatServer, DashboardServer, UgcServer, WorldServer
		std::optional<uint32_t> zone, clone, instance;
		int64_t started{}; // Unix seconds, 0 when the name has none
	};
	std::optional<LogName> ParseLogName(std::string_view fileName);
	std::optional<LogName> ParseCrashName(std::string_view fileName);

	// Short names used in queries and on the page (master, auth, chat, dashboard, ugc, world) to server names, and back
	std::optional<std::string> ServerFromShortName(std::string_view name);

	struct Filter {
		int64_t from{}, to{}; // Unix seconds; 0 leaves that end open
		std::set<std::string> servers; // server names; empty: all of them
		std::set<uint32_t> zones; // world servers of these zones; empty: all
		std::optional<uint32_t> clone, instance;
		bool crashDumps{};
		bool trim{}; // only the lines written in the date range, rather than whole files
		std::string text; // only lines with this in them (case doesn't matter)
		bool redactIps{};

		// Query string values: from, to (Unix seconds), servers (comma separated short names), zones, clone, instance,
		// crash, trim, redact (1 for on), text. Sets `error` and returns nullopt when a value is wrong.
		static std::optional<Filter> FromQuery(const std::function<std::string(const std::string&)>& value, std::string& error);
		nlohmann::json ToJson() const;
		// Whether only some lines of a file go in
		bool FiltersLines() const { return trim || !text.empty() || redactIps; }
	};

	struct File {
		std::filesystem::path path;
		std::string archiveName; // logs/<path under logs>, or crash_dumps/<name>
		LogName name;
		int64_t started{}; // from the name, or when it was last written if the name has none
		int64_t written{}; // last written, Unix seconds
		uintmax_t size{};
		bool crashDump{};
	};

	// The files matching `filter` (a file matches a date range when the time from its start to its last write
	// overlaps it), oldest first. `dumpFolder` may be empty (no crash dumps).
	std::vector<File> Select(const std::filesystem::path& logFolder, const std::filesystem::path& dumpFolder, const Filter& filter);

	// The time at the start of a log line ("[27-09-26 09:49:54 ..." in the server's local time), if it has one
	std::optional<int64_t> LineTime(std::string_view line);

	// `line` with IPv4 and IPv6 addresses replaced by [ip]
	std::string RedactIps(std::string_view line);

	struct Result {
		bool ok{};
		std::string error;
		bool overLimit{};
		size_t files{}; // files put in (files with no lines left are left out)
		uint64_t bytesIn{}; // uncompressed, what the files put in hold after filtering
		uint64_t archiveSize{};
	};

	/**
	 * Write a zip file of `files` (filtered as `filter` says) and a manifest.txt that starts with `manifestHeader` to
	 * `out`. Gives up once more than `maxBytes` (uncompressed) would go in, or when `cancelled` says so. Reads and
	 * compresses a piece at a time. On failure `out` is removed.
	 */
	Result WriteZip(const std::filesystem::path& out, const std::vector<File>& files, const Filter& filter,
		const std::string& manifestHeader, uint64_t maxBytes, const std::function<bool()>& cancelled = {});

	// "1.5 MB" for manifests and errors
	std::string SizeText(uint64_t bytes);
}
