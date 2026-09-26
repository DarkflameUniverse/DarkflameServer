#include "BackupFiles.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <set>

#include "sqlite3.h"

namespace {
	const std::regex BACKUP_NAME("^dlu-[0-9]{8}-[0-9]{6}\\.(sqlite|sql)$");
	constexpr std::string_view PARTIAL = ".partial";

	// Tables every DarkflameServer database has; a backup without them is not a server database
	const std::vector<std::string> REQUIRED_TABLES = { "accounts", "charinfo", "migration_history" };

	struct Statement {
		sqlite3_stmt* stmt{};
		Statement(sqlite3* db, const std::string& sql) { sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr); }
		~Statement() { sqlite3_finalize(stmt); }
		Statement(const Statement&) = delete;
		Statement& operator=(const Statement&) = delete;
		bool Step() { return stmt && sqlite3_step(stmt) == SQLITE_ROW; }
		std::string Text(int column) const {
			const auto* text = sqlite3_column_text(stmt, column);
			return text ? reinterpret_cast<const char*>(text) : "";
		}
		int64_t Int(int column) const { return sqlite3_column_int64(stmt, column); }
	};

	std::string QuoteIdentifier(const std::string& name) {
		std::string out = "\"";
		for (const char c : name) out += c == '"' ? std::string("\"\"") : std::string(1, c);
		return out + "\"";
	}

	void CheckRequiredTables(BackupFiles::Verification& result, std::vector<std::string>& problems) {
		for (const auto& required : REQUIRED_TABLES) {
			if (std::none_of(result.tables.begin(), result.tables.end(), [&](const auto& table) { return table.first == required; })) {
				problems.push_back("no " + required + " table");
			}
		}
	}

	// A file: URI for sqlite3_open_v2; '?', '#' and '%' in the path must be escaped
	std::string UriPath(const std::filesystem::path& file) {
		std::string out = file.has_root_name() ? "file:/" : "file:";
		for (const char c : file.generic_string()) {
			if (c == '?') out += "%3f";
			else if (c == '#') out += "%23";
			else if (c == '%') out += "%25";
			else out += c;
		}
		return out;
	}

	std::string Join(const std::vector<std::string>& parts) {
		std::string out;
		for (const auto& part : parts) out += (out.empty() ? "" : "; ") + part;
		return out;
	}
}

namespace BackupFiles {
	std::string Name(std::time_t when, bool mysql) {
		std::tm utc{};
#ifdef _WIN32
		gmtime_s(&utc, &when);
#else
		gmtime_r(&when, &utc);
#endif
		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &utc);
		return std::string("dlu-") + buffer + (mysql ? ".sql" : ".sqlite");
	}

	bool IsName(std::string_view name) {
		return std::regex_match(name.begin(), name.end(), BACKUP_NAME);
	}

	std::string PartialName(const std::string& name) {
		return name + std::string(PARTIAL);
	}

	bool IsPartialName(std::string_view name) {
		return name.size() > PARTIAL.size() && name.ends_with(PARTIAL) && IsName(name.substr(0, name.size() - PARTIAL.size()));
	}

	std::vector<std::string> Expired(std::vector<std::string> names, int64_t keep) {
		std::erase_if(names, [](const std::string& name) { return !IsName(name); });
		if (keep <= 0 || names.size() <= static_cast<size_t>(keep)) return {};
		// The name starts with the UTC time, so name order is age order; newest first
		std::sort(names.begin(), names.end(), std::greater<>());
		return { names.begin() + keep, names.end() };
	}

	std::string SqlString(std::string_view value) {
		std::string out = "'";
		for (const char c : value) out += c == '\'' ? std::string("''") : std::string(1, c);
		return out + "'";
	}

	std::string MysqlOptionFile(std::string_view user, std::string_view password) {
		// Option file values in double quotes take backslash escapes
		const auto quote = [](std::string_view value) {
			std::string out = "\"";
			for (const char c : value) {
				if (c == '\n' || c == '\r') continue;
				if (c == '"' || c == '\\') out += '\\';
				out += c;
			}
			return out + "\"";
		};
		return "[client]\nuser=" + quote(user) + "\npassword=" + quote(password) + "\n";
	}

	std::vector<std::string> MysqldumpArguments(const DumpCommand& command) {
		auto host = command.host;
		std::string port;
		if (const auto colon = host.find(':'); colon != std::string::npos) {
			port = host.substr(colon + 1);
			host = host.substr(0, colon);
		}
		std::vector<std::string> arguments{ command.program, "--defaults-extra-file=" + command.optionsFile,
			"--single-transaction", "--quick", "--routines", "--triggers", "--hex-blob", "--no-tablespaces", "--default-character-set=utf8mb4" };
		if (!host.empty()) arguments.push_back("--host=" + host);
		if (!port.empty()) arguments.push_back("--port=" + port);
		arguments.push_back("--result-file=" + command.target);
		if (!command.errorFile.empty()) arguments.push_back("--log-error=" + command.errorFile);
		arguments.push_back(command.database);
		return arguments;
	}

	nlohmann::json Verification::ToJson() const {
		nlohmann::json tableList = nlohmann::json::array();
		for (const auto& [name, rows] : tables) tableList.push_back({ {"name", name}, {"rows", rows} });
		nlohmann::json out = { {"ok", ok}, {"summary", summary}, {"format", format}, {"integrity", integrity}, {"tables", tableList},
			{"rows", rows}, {"migrations", migrations}, {"last_migration", lastMigration} };
		if (twoFactorChecked) out["two_factor"] = { {"accounts", twoFactorAccounts}, {"unreadable", twoFactorUnreadable} };
		return out;
	}

	Verification VerifySqlite(const std::filesystem::path& file, const CanDecrypt& canDecrypt) {
		Verification result;
		result.format = "sqlite";
		sqlite3* db = nullptr;
		// Read-only and immutable: nothing is written next to the backup (no -wal or -shm) and it cannot be changed
		const auto uri = UriPath(file) + "?mode=ro&immutable=1";
		if (sqlite3_open_v2(uri.c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, nullptr) != SQLITE_OK) {
			result.summary = std::string("Cannot open: ") + (db ? sqlite3_errmsg(db) : "out of memory");
			sqlite3_close(db);
			return result;
		}

		std::vector<std::string> problems;
		{
			Statement check(db, "PRAGMA integrity_check(10)");
			if (!check.stmt) problems.push_back(std::string("not a SQLite database (") + sqlite3_errmsg(db) + ")");
			while (check.Step()) result.integrity += (result.integrity.empty() ? "" : "\n") + check.Text(0);
			if (check.stmt && result.integrity != "ok") problems.push_back("integrity check: " + result.integrity.substr(0, result.integrity.find('\n')));
		}

		std::vector<std::string> names;
		{
			Statement tables(db, "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name");
			while (tables.Step()) names.push_back(tables.Text(0));
		}
		for (const auto& name : names) {
			Statement count(db, "SELECT COUNT(*) FROM " + QuoteIdentifier(name));
			if (!count.Step()) {
				problems.push_back("cannot read " + name);
				continue;
			}
			result.tables.emplace_back(name, count.Int(0));
			result.rows += count.Int(0);
		}
		CheckRequiredTables(result, problems);

		{
			Statement migrations(db, "SELECT COUNT(*), (SELECT name FROM migration_history ORDER BY rowid DESC LIMIT 1) FROM migration_history");
			if (migrations.Step()) {
				result.migrations = migrations.Int(0);
				result.lastMigration = migrations.Text(1);
			}
		}

		if (canDecrypt) {
			Statement secrets(db, "SELECT totp_secret FROM accounts WHERE totp_enabled_at != 0 AND totp_secret IS NOT NULL AND totp_secret != ''");
			if (secrets.stmt) {
				result.twoFactorChecked = true;
				while (secrets.Step()) {
					result.twoFactorAccounts++;
					if (!canDecrypt(secrets.Text(0))) result.twoFactorUnreadable++;
				}
			}
		}
		sqlite3_close(db);

		result.ok = problems.empty();
		result.summary = result.ok
			? "OK: " + std::to_string(result.tables.size()) + " tables, " + std::to_string(result.rows) + " rows, " +
				std::to_string(result.migrations) + " migrations"
			: Join(problems);
		if (result.twoFactorUnreadable > 0) {
			result.summary += "; " + std::to_string(result.twoFactorUnreadable) + " of " + std::to_string(result.twoFactorAccounts) +
				" two-factor secrets cannot be decrypted with this server's two-factor key (restore the matching dashboard_totp_key or totp_key)";
		}
		return result;
	}

	Verification VerifyDump(const std::filesystem::path& file) {
		Verification result;
		result.format = "mysqldump";
		std::ifstream in(file, std::ios::binary);
		if (!in) {
			result.summary = "Cannot open the file";
			return result;
		}

		std::vector<std::string> problems;
		bool header = false;
		bool junkBeforeHeader = false;
		std::string lastLine;
		std::string line;
		std::vector<std::string> order;
		std::map<std::string, int64_t> inserts;
		const auto tableName = [](const std::string& text, size_t from) -> std::string {
			if (from >= text.size() || text[from] != '`') return {};
			const auto end = text.find('`', from + 1);
			return end == std::string::npos ? std::string() : text.substr(from + 1, end - from - 1);
		};
		for (size_t number = 0; std::getline(in, line); number++) {
			if (!line.empty() && line.back() == '\r') line.pop_back();
			// MariaDB puts a sandbox-mode comment before the header
			// Only comments may come before the header; anything else (a warning printed into the file) breaks a restore
			if (!header && !junkBeforeHeader && number < 5) {
				if (line.starts_with("-- MySQL dump") || line.starts_with("-- MariaDB dump")) header = true;
				else if (!line.empty() && !line.starts_with("/*") && !line.starts_with("--")) junkBeforeHeader = true;
			}
			if (line.starts_with("CREATE TABLE ")) {
				const auto name = tableName(line, 13);
				if (!name.empty() && !inserts.contains(name)) { order.push_back(name); inserts[name] = 0; }
			} else if (line.starts_with("INSERT INTO ")) {
				const auto name = tableName(line, 12);
				if (!name.empty()) inserts[name]++;
			}
			if (!line.empty()) lastLine = line;
		}
		if (!header) problems.push_back("does not start like mysqldump output (was something else written into it?)");
		if (!lastLine.starts_with("-- Dump completed")) problems.push_back("incomplete: no \"-- Dump completed\" at the end");
		for (const auto& name : order) {
			result.tables.emplace_back(name, inserts[name]);
			result.rows += inserts[name];
		}
		CheckRequiredTables(result, problems);
		result.ok = problems.empty();
		result.summary = result.ok ? "OK: complete dump of " + std::to_string(result.tables.size()) + " tables" : Join(problems);
		return result;
	}
}
