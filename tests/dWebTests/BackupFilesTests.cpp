#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>

#include "BackupFiles.h"
#include "Totp.h"
#include "sqlite3.h"

namespace fs = std::filesystem;

namespace {
	class TempFolder {
	public:
		TempFolder() {
			std::random_device random;
			path = fs::temp_directory_path() / ("dlu-backup-test-" + std::to_string(random()));
			fs::create_directories(path);
		}
		~TempFolder() { std::error_code ec; fs::remove_all(path, ec); }
		fs::path path;
	};

	void Exec(sqlite3* db, const std::string& sql) {
		char* error = nullptr;
		ASSERT_EQ(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error), SQLITE_OK) << (error ? error : "") << " in " << sql;
	}

	// A small database shaped like the server's, in WAL mode like the servers use
	sqlite3* MakeDatabase(const fs::path& file) {
		sqlite3* db = nullptr;
		EXPECT_EQ(sqlite3_open(file.string().c_str(), &db), SQLITE_OK);
		Exec(db, "PRAGMA journal_mode = WAL; PRAGMA wal_autocheckpoint = 0;");
		Exec(db, "CREATE TABLE accounts (id INTEGER PRIMARY KEY, name TEXT, totp_secret TEXT NULL, totp_enabled_at BIGINT NOT NULL DEFAULT 0);");
		Exec(db, "CREATE TABLE charinfo (id INTEGER PRIMARY KEY, account_id INT, name TEXT);");
		Exec(db, "CREATE TABLE migration_history (name TEXT NOT NULL, date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP);");
		Exec(db, "INSERT INTO migration_history (name) VALUES ('0_initial.sql'), ('10_second.sql'), ('9_third.sql');");
		return db;
	}

	void WriteFile(const fs::path& file, const std::string& text) {
		std::ofstream(file, std::ios::binary) << text;
	}

	const std::string DUMP_BODY =
		"-- MariaDB dump 10.19  Distrib 10.11.6-MariaDB, for Linux (x86_64)\n"
		"--\n-- Host: localhost    Database: dlu\n"
		"CREATE TABLE `accounts` (\n  `id` int(10) unsigned NOT NULL AUTO_INCREMENT\n);\n"
		"INSERT INTO `accounts` VALUES\n(1),\n(2);\n"
		"CREATE TABLE `charinfo` (\n  `id` bigint NOT NULL\n);\n"
		"CREATE TABLE `migration_history` (\n  `name` text NOT NULL\n);\n"
		"INSERT INTO `migration_history` VALUES\n('0_initial.sql');\n"
		"/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;\n\n";
}

TEST(BackupFilesTests, NamesUseUtcTimeAndOnlyTheirOwnPattern) {
	// 2026-09-26 04:14:55 UTC
	EXPECT_EQ(BackupFiles::Name(1790396095, false), "dlu-20260926-041455.sqlite");
	EXPECT_EQ(BackupFiles::Name(1790396095, true), "dlu-20260926-041455.sql");
	EXPECT_TRUE(BackupFiles::IsName("dlu-20260926-041455.sqlite"));
	EXPECT_TRUE(BackupFiles::IsName("dlu-20260926-041455.sql"));
	EXPECT_FALSE(BackupFiles::IsName("dlu-20260926-041455.sqlite.partial"));
	EXPECT_FALSE(BackupFiles::IsName("../dlu-20260926-041455.sql"));
	EXPECT_FALSE(BackupFiles::IsName("dlu-2026-041455.sql"));
	EXPECT_FALSE(BackupFiles::IsName("game.sqlite"));

	EXPECT_EQ(BackupFiles::PartialName("dlu-20260926-041455.sql"), "dlu-20260926-041455.sql.partial");
	EXPECT_TRUE(BackupFiles::IsPartialName("dlu-20260926-041455.sqlite.partial"));
	EXPECT_FALSE(BackupFiles::IsPartialName("dlu-20260926-041455.sqlite"));
	EXPECT_FALSE(BackupFiles::IsPartialName("game.sqlite.partial"));
	EXPECT_FALSE(BackupFiles::IsPartialName(".partial"));
}

TEST(BackupFilesTests, ExpiredKeepsTheNewest) {
	const std::vector<std::string> names = { "dlu-20260102-000000.sqlite", "dlu-20260104-000000.sqlite", "notes.txt",
		"dlu-20260101-235959.sql", "dlu-20260103-120000.sqlite", "dlu-20260105-000000.sqlite.partial" };
	EXPECT_TRUE(BackupFiles::Expired(names, 0).empty()); // 0 keeps all
	EXPECT_TRUE(BackupFiles::Expired(names, 4).empty());
	EXPECT_TRUE(BackupFiles::Expired(names, 100).empty());
	EXPECT_EQ(BackupFiles::Expired(names, 2), (std::vector<std::string>{ "dlu-20260102-000000.sqlite", "dlu-20260101-235959.sql" }));
	EXPECT_EQ(BackupFiles::Expired(names, 1),
		(std::vector<std::string>{ "dlu-20260103-120000.sqlite", "dlu-20260102-000000.sqlite", "dlu-20260101-235959.sql" }));
	EXPECT_TRUE(BackupFiles::Expired(names, -3).empty());
}

TEST(BackupFilesTests, Quoting) {
	EXPECT_EQ(BackupFiles::SqlString("/srv/it's/dlu.sqlite"), "'/srv/it''s/dlu.sqlite'");
	EXPECT_EQ(BackupFiles::MysqlOptionFile("dlu", "p\"a\\ss\nx"), "[client]\nuser=\"dlu\"\npassword=\"p\\\"a\\\\ssx\"\n");
}

TEST(BackupFilesTests, VacuumIntoCopyOfALiveWalDatabaseVerifies) {
	TempFolder folder;
	auto* db = MakeDatabase(folder.path / "live.sqlite");
	Exec(db, "BEGIN; INSERT INTO accounts (name) VALUES ('a'), ('b'); INSERT INTO charinfo (account_id, name) VALUES (1, 'c'); COMMIT;");
	// Committed but still only in the -wal file: a plain copy of live.sqlite would not have these rows
	const auto backup = folder.path / BackupFiles::PartialName("dlu-20260926-041455.sqlite");
	Exec(db, "VACUUM INTO " + BackupFiles::SqlString(backup.string()) + ";");

	const auto check = BackupFiles::VerifySqlite(backup);
	EXPECT_TRUE(check.ok) << check.summary;
	EXPECT_EQ(check.integrity, "ok");
	EXPECT_EQ(check.rows, 6); // 2 accounts, 1 character, 3 migrations
	EXPECT_EQ(check.migrations, 3);
	EXPECT_EQ(check.lastMigration, "9_third.sql"); // the last one applied, not the highest name
	ASSERT_EQ(check.tables.size(), 3u);
	EXPECT_EQ(check.tables[0], std::make_pair(std::string("accounts"), int64_t{ 2 }));
	EXPECT_FALSE(check.twoFactorChecked);
	EXPECT_FALSE(fs::exists(folder.path / (backup.filename().string() + "-wal"))); // opened read-only, nothing written beside it
	EXPECT_EQ(check.ToJson()["tables"][1]["name"], "charinfo");
	sqlite3_close(db);
}

TEST(BackupFilesTests, VerifyReportsTwoFactorSecretsTheKeyCannotRead) {
	TempFolder folder;
	Totp::SetKeyForTesting(std::vector<uint8_t>(32, 7));
	const auto secret = Totp::EncryptSecret(Totp::GenerateSecret());
	ASSERT_TRUE(secret);
	auto* db = MakeDatabase(folder.path / "live.sqlite");
	Exec(db, "INSERT INTO accounts (name, totp_secret, totp_enabled_at) VALUES ('a', '" + *secret + "', 5), ('b', 'set up but not on', 0), ('c', NULL, 0);");
	const auto backup = folder.path / "dlu-20260926-041455.sqlite";
	Exec(db, "VACUUM INTO " + BackupFiles::SqlString(backup.string()) + ";");
	sqlite3_close(db);

	const auto canDecrypt = [](const std::string& stored) { return Totp::DecryptSecret(stored).has_value(); };
	auto check = BackupFiles::VerifySqlite(backup, canDecrypt);
	EXPECT_TRUE(check.ok);
	EXPECT_TRUE(check.twoFactorChecked);
	EXPECT_EQ(check.twoFactorAccounts, 1);
	EXPECT_EQ(check.twoFactorUnreadable, 0);

	// Restored next to a different key: the database is fine, but that account cannot sign in
	Totp::SetKeyForTesting(std::vector<uint8_t>(32, 9));
	check = BackupFiles::VerifySqlite(backup, canDecrypt);
	EXPECT_TRUE(check.ok);
	EXPECT_EQ(check.twoFactorUnreadable, 1);
	EXPECT_NE(check.summary.find("two-factor"), std::string::npos);
	EXPECT_EQ(check.ToJson()["two_factor"]["unreadable"], 1);
}

TEST(BackupFilesTests, VerifyRejectsDamagedOrForeignFiles) {
	TempFolder folder;
	auto* db = MakeDatabase(folder.path / "live.sqlite");
	Exec(db, "CREATE TABLE filler (x BLOB); WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 400) "
		"INSERT INTO filler SELECT randomblob(900) FROM n; CREATE INDEX filler_x ON filler (x);");
	const auto good = folder.path / "good.sqlite";
	Exec(db, "VACUUM INTO " + BackupFiles::SqlString(good.string()) + ";");
	sqlite3_close(db);
	ASSERT_TRUE(BackupFiles::VerifySqlite(good).ok);

	// Scribble over pages in the middle of the file
	const auto damaged = folder.path / "damaged.sqlite";
	fs::copy_file(good, damaged);
	{
		std::fstream file(damaged, std::ios::in | std::ios::out | std::ios::binary);
		file.seekp(4096 * 5);
		const std::string junk(4096 * 20, '\x5a');
		file.write(junk.data(), static_cast<std::streamsize>(junk.size()));
	}
	EXPECT_FALSE(BackupFiles::VerifySqlite(damaged).ok);

	const auto text = folder.path / "text.sqlite";
	WriteFile(text, "not a database at all, just some text that is long enough to have a header");
	EXPECT_FALSE(BackupFiles::VerifySqlite(text).ok);
	EXPECT_FALSE(BackupFiles::VerifySqlite(folder.path / "missing.sqlite").ok);

	// A valid SQLite file that is not a server database
	sqlite3* other = nullptr;
	ASSERT_EQ(sqlite3_open((folder.path / "other.sqlite").string().c_str(), &other), SQLITE_OK);
	Exec(other, "CREATE TABLE accounts (id INTEGER);");
	sqlite3_close(other);
	const auto check = BackupFiles::VerifySqlite(folder.path / "other.sqlite");
	EXPECT_FALSE(check.ok);
	EXPECT_NE(check.summary.find("no charinfo table"), std::string::npos);
}

TEST(BackupFilesTests, VerifyDump) {
	TempFolder folder;
	const auto complete = folder.path / "complete.sql";
	WriteFile(complete, "/*M!999999\\- enable the sandbox mode */ \n" + DUMP_BODY + "-- Dump completed on 2026-09-26  3:30:01\n");
	auto check = BackupFiles::VerifyDump(complete);
	EXPECT_TRUE(check.ok) << check.summary;
	ASSERT_EQ(check.tables.size(), 3u);
	EXPECT_EQ(check.tables[0], std::make_pair(std::string("accounts"), int64_t{ 1 }));
	EXPECT_EQ(check.tables[1], std::make_pair(std::string("charinfo"), int64_t{ 0 }));

	// What `mysqldump ... > file 2>&1` wrote on MariaDB 11: the deprecation warning ahead of the SQL
	const auto noisy = folder.path / "noisy.sql";
	WriteFile(noisy, "mysqldump: Deprecated program name. It will be removed in a future release, use '/usr/bin/mariadb-dump' instead\n" +
		DUMP_BODY + "-- Dump completed on 2026-09-26  3:30:01\n");
	EXPECT_FALSE(BackupFiles::VerifyDump(noisy).ok);

	const auto cut = folder.path / "cut.sql";
	WriteFile(cut, DUMP_BODY);
	check = BackupFiles::VerifyDump(cut);
	EXPECT_FALSE(check.ok);
	EXPECT_NE(check.summary.find("incomplete"), std::string::npos);

	EXPECT_FALSE(BackupFiles::VerifyDump(folder.path / "missing.sql").ok);
}
