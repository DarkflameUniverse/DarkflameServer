#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "json.hpp"

/**
 * The parts of database backups that do not need the dashboard: file names, which old backups to delete, the
 * mysqldump command line, and checking that a backup file can be restored. Kept separate so they can be unit tested.
 */
namespace BackupFiles {
	// dlu-YYYYMMDD-HHMMSS.sqlite (SQLite, VACUUM INTO) or .sql (MySQL, mysqldump), UTC
	std::string Name(std::time_t when, bool mysql);
	// Only files named like Name() are listed, downloaded, deleted or verified
	bool IsName(std::string_view name);
	// Backups are written under this name and renamed when complete, so a half-written file is never listed or kept
	std::string PartialName(const std::string& name);
	bool IsPartialName(std::string_view name);

	/**
	 * Which backups to delete so only the newest `keep` remain (0 keeps all). Names that are not backups are ignored.
	 * @return the names to delete, oldest last
	 */
	std::vector<std::string> Expired(std::vector<std::string> names, int64_t keep);

	// A SQL string literal: 'it''s'
	std::string SqlString(std::string_view value);

	// A [client] option file with the login, so the password is never on the command line
	std::string MysqlOptionFile(std::string_view user, std::string_view password);

	struct DumpCommand {
		std::string program;
		std::string optionsFile;
		std::string host; // host or host:port
		std::string database;
		std::string target;
		std::string errorFile; // mysqldump's messages; never mixed into the dump, where they would break a restore
	};
	// mysqldump as an argument vector for Process::Run (no shell); its messages go to errorFile through --log-error
	std::vector<std::string> MysqldumpArguments(const DumpCommand& command);

	struct Verification {
		bool ok{};
		std::string summary;
		std::string format;    // "sqlite" or "mysqldump"
		std::string integrity; // SQLite: PRAGMA integrity_check's first lines ("ok")
		std::vector<std::pair<std::string, int64_t>> tables; // name, rows (SQLite) or INSERT statements (dump)
		int64_t rows{};
		int64_t migrations{};
		std::string lastMigration;
		// Accounts with two-factor login on, and how many of their secrets the current key could not decrypt
		int64_t twoFactorAccounts{};
		int64_t twoFactorUnreadable{};
		bool twoFactorChecked{};

		nlohmann::json ToJson() const;
	};

	using CanDecrypt = std::function<bool(const std::string& storedSecret)>;

	/**
	 * Open a SQLite backup read-only and check it: PRAGMA integrity_check, every table's row count, migration_history,
	 * and (with canDecrypt) whether each two-factor secret in it can be decrypted with the current key.
	 */
	Verification VerifySqlite(const std::filesystem::path& file, const CanDecrypt& canDecrypt = {});

	/**
	 * Check a mysqldump file is complete: starts with a dump header, ends with "-- Dump completed", and has
	 * the tables the servers need (accounts, charinfo, migration_history).
	 */
	Verification VerifyDump(const std::filesystem::path& file);
}
