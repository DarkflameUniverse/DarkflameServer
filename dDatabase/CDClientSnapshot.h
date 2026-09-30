#ifndef CDCLIENTSNAPSHOT_H
#define CDCLIENTSNAPSHOT_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "FdbSnapshot.h"

/**
 * Makes a new copy of the client's cdclient.fdb and the CDServer.sqlite that goes with it (see FdbSnapshot.h).
 *
 * Everything here uses its own files and SQLite connection and never logs, so master runs it on a worker thread and
 * logs the result on the main thread.
 */
namespace CDClientSnapshot {
	struct Result {
		bool ok = false;
		// The copy has the same bytes as the current one; nothing to do
		bool unchanged = false;
		std::string error;
		uint64_t hash = 0;
		FdbSnapshot::Current current;
		// What the new fdb changed, one line per table (FdbSnapshot::DescribeChanges)
		std::vector<std::string> changes;
		// The cdserver migrations applied to the new CDServer.sqlite
		std::vector<std::string> migrations;
	};

	/**
	 * Converts fdb into a new SQLite file at out and applies every cdserver migration in migrationsDir to it. It is
	 * written under a temporary name and renamed to out when complete.
	 */
	bool MakeSqlite(const std::filesystem::path& fdb, const std::filesystem::path& out, const std::filesystem::path& migrationsDir,
		std::vector<std::string>& migrations, std::string& error);

	/**
	 * Copies clientFdb into resServer and, if it differs from currentHash, makes its CDServer-<hash>.sqlite.
	 *
	 * @param previousFdb The copy in use now, compared with the new one for Result::changes (may be empty)
	 */
	Result Build(const std::filesystem::path& clientFdb, const std::filesystem::path& resServer, const std::filesystem::path& migrationsDir,
		std::optional<uint64_t> currentHash, const std::filesystem::path& previousFdb);
};

#endif // CDCLIENTSNAPSHOT_H
