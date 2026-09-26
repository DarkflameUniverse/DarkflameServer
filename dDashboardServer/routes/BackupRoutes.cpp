#include "BackupRoutes.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <bcrypt/bcrypt.h>

#include "RouteUtils.h"
#include "BackupFiles.h"
#include "Totp.h"
#include "WSRoutes.h"
#include "Scheduler.h"
#include "Background.h"
#include "Alerts.h"
#include "DashboardAuthService.h"
#include "BinaryPathFinder.h"
#include "Process.h"
#include "Database.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"
#include "GeneralUtils.h"
#include "eHTTPMethod.h"

using namespace RouteUtils;

namespace {
	namespace fs = std::filesystem;

	constexpr int64_t LINK_SECONDS = 60;

	struct DownloadLink {
		std::string file;
		uint32_t accountId{};
		int64_t expires{};
	};
	std::map<std::string, DownloadLink> g_Links; // token hash -> link

	// The last check of each backup (POST /api/backups/:name/verify or the check after making it), until restart
	struct Check {
		nlohmann::json result;
		int64_t at{};
		std::string by;
	};
	std::map<std::string, Check> g_Checks;

	fs::path Folder() {
		fs::path folder = Game::config->GetValue("backup_folder");
		if (folder.empty()) folder = "backups";
		return folder.is_absolute() ? folder : BinaryPathFinder::GetBinaryDir() / folder;
	}

	bool IsMySQL() {
		return Game::config->GetValue("database_type") == "mysql";
	}

	// Backups, newest first
	std::vector<fs::directory_entry> ListBackups() {
		std::vector<fs::directory_entry> files;
		std::error_code ec;
		for (const auto& entry : fs::directory_iterator(Folder(), ec)) {
			if (entry.is_regular_file(ec) && BackupFiles::IsName(entry.path().filename().string())) files.push_back(entry);
		}
		std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.path().filename() > b.path().filename(); });
		return files;
	}

	// A backup holds every account's password hash: only the server's own user may read it
	void MakePrivate(const fs::path& path, bool folder) {
		std::error_code ignored;
		fs::permissions(path, folder ? fs::perms::owner_all : fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, ignored);
	}

	// A temporary file only this user can read. On Unix mkstemp also picks a name nobody could have planted a link at.
	fs::path WritePrivateTempFile(const std::string& contents) {
#ifndef _WIN32
		std::string name = (fs::temp_directory_path() / "dlu-backup-XXXXXX").string();
		const int fd = ::mkstemp(name.data());
		if (fd < 0) throw std::runtime_error("could not create a temporary file");
		const bool written = ::write(fd, contents.data(), contents.size()) == static_cast<ssize_t>(contents.size());
		::close(fd);
		if (!written) throw std::runtime_error("could not write the temporary file");
		return name;
#else
		const auto path = fs::temp_directory_path() / ("dlu-backup-" + GenerateUrlToken().substr(0, 16) + ".cnf");
		std::ofstream(path, std::ios::binary) << contents;
		fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
		return path;
#endif
	}

	std::string ReadSmallFile(const fs::path& path, size_t limit = 2000) {
		std::ifstream in(path, std::ios::binary);
		std::string text(limit, '\0');
		in.read(text.data(), static_cast<std::streamsize>(limit));
		text.resize(static_cast<size_t>(in.gcount()));
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
		return text;
	}

	// Whether a two-factor secret from a backup can be decrypted with this server's key. Worker-thread safe: the key is
	// loaded once at start and only read.
	bool CanDecryptSecret(const std::string& stored) {
		return Totp::DecryptSecret(stored).has_value();
	}

	// Runs on the background worker: check a backup file (no config, no Database::Get())
	BackupFiles::Verification VerifyFile(const fs::path& file, bool dump) {
		return dump ? BackupFiles::VerifyDump(file) : BackupFiles::VerifySqlite(file, CanDecryptSecret);
	}

	void RememberCheck(const std::string& name, const nlohmann::json& result, const std::string& by) {
		g_Checks[name] = { result, static_cast<int64_t>(std::time(nullptr)), by };
		std::erase_if(g_Checks, [](const auto& entry) { std::error_code ec; return !fs::exists(Folder() / entry.first, ec); });
	}

	/**
	 * What a backup does not contain but a restore needs: these live next to the server binaries, not in the database.
	 * Two-factor secrets in the database are encrypted with the two-factor key and cannot be read without it.
	 */
	nlohmann::json FilesToKeep() {
		const auto dir = BinaryPathFinder::GetBinaryDir();
		nlohmann::json files = nlohmann::json::array();
		const auto add = [&](const std::string& name, const fs::path& path, bool needed, const std::string& why) {
			std::error_code ec;
			files.push_back({ {"name", name}, {"path", path.string()}, {"present", fs::exists(path, ec)}, {"needed", needed}, {"why", why} });
		};
		if (Game::config->GetValue("totp_key").empty()) {
			add("dashboard_totp_key", dir / "dashboard_totp_key", true,
				"Decrypts the two-factor secrets in the database. Without it everyone with two-factor login has to be reset.");
		} else {
			add("totp_key in dashboardconfig.ini", dir / "dashboardconfig.ini", true,
				"Decrypts the two-factor secrets in the database. Without it everyone with two-factor login has to be reset.");
		}
		if (Game::config->GetValue("jwt_secret").empty()) {
			add("dashboard_jwt_secret", dir / "dashboard_jwt_secret", false, "Signs sign-ins; without it everyone just signs in again.");
		}
		const auto token = dir / "dashboard_oauth2_token.json";
		std::error_code ec;
		if (fs::exists(token, ec)) add("dashboard_oauth2_token.json", token, false, "The mail account connected with OAuth2; without it, connect it again.");
		add("*.ini", dir, true, "Server settings (database connection, ports, keys). Settings changed on the Settings page are in the database.");
		add("vanity/", dir / "vanity", false, "NPCs and objects placed with vanity files.");
		return files;
	}

	void RunBackup(Scheduler::RunPtr run) {
		const auto folder = Folder();
		std::error_code ec;
		const bool created = fs::create_directories(folder, ec);
		if (ec) return run->Finish(false, "Could not create " + folder.string() + ": " + ec.message());
		if (created) MakePrivate(folder, true);
		// Left behind by a backup that was interrupted (the dashboard stopped); never a usable backup
		for (const auto& entry : fs::directory_iterator(folder, ec)) {
			if (BackupFiles::IsPartialName(entry.path().filename().string())) {
				std::error_code ignored;
				if (fs::remove(entry.path(), ignored)) run->Log("Removed unfinished " + entry.path().filename().string());
			}
		}
		const bool mysql = IsMySQL();
		const auto name = BackupFiles::Name(std::time(nullptr), mysql);
		const auto target = folder / name;
		const auto partial = folder / BackupFiles::PartialName(name);
		if (fs::exists(target, ec)) return run->Finish(false, name + " already exists; try again in a second");
		const auto keep = std::max<int64_t>(GeneralUtils::TryParse<int64_t>(Game::config->GetValue("backup_keep")).value_or(7), 0);
		run->Log("Writing " + target.string());

		// Settings are read here on the main thread; the worker only gets plain values
		struct MySQLLogin { std::string program, host, database, user, password; };
		std::optional<MySQLLogin> login;
		if (mysql) {
			auto program = Game::config->GetValue("backup_mysqldump");
			login = MySQLLogin{ program.empty() ? "mysqldump" : program, Game::config->GetValue("mysql_host"), Game::config->GetValue("mysql_database"),
				Game::config->GetValue("mysql_username"), Game::config->GetValue("mysql_password") };
		}

		const auto started = std::chrono::steady_clock::now();
		const bool queued = Background::Run("database_backup", [target, partial, login](GameDatabase& db) -> nlohmann::json {
			nlohmann::json result;
			std::error_code ignored;
			if (!login) {
				// VACUUM INTO reads one consistent snapshot through SQLite, so it is safe while the servers write. (A plain
				// copy of the file is not: with WAL, recent commits are only in the -wal file.)
				db.ExecuteCustomQuery("VACUUM INTO " + BackupFiles::SqlString(partial.string()) + ";");
			} else {
				// The password goes in a private option file, never on the command line where other users could see it
				const auto options = WritePrivateTempFile(BackupFiles::MysqlOptionFile(login->user, login->password));
				const auto errors = WritePrivateTempFile("");
				// Run without a shell; the program path comes from the config file only (backup_mysqldump is file-only)
				if (Process::HasShellMetacharacters(login->program)) {
					fs::remove(options, ignored);
					fs::remove(errors, ignored);
					throw std::runtime_error("backup_mysqldump contains characters that are not allowed in a program path");
				}
				const auto arguments = BackupFiles::MysqldumpArguments({ login->program, options.string(), login->host, login->database,
					partial.string(), errors.string() });
				const int status = Process::Run(arguments);
				const auto messages = ReadSmallFile(errors);
				fs::remove(options, ignored);
				fs::remove(errors, ignored);
				if (status != 0) {
					fs::remove(partial, ignored);
					throw std::runtime_error("mysqldump failed" + (messages.empty() ? std::string() : ": " + messages));
				}
				if (!messages.empty()) result["messages"] = messages;
			}
			MakePrivate(partial, false);

			// Check the new file before it replaces anything: a backup that cannot be restored must not push out a good one
			const auto check = VerifyFile(partial, login.has_value());
			if (!check.ok) {
				fs::remove(partial, ignored);
				throw std::runtime_error("the new backup failed its check (" + check.summary + "); older backups were kept");
			}
			fs::rename(partial, target);
			result["check"] = check.ToJson();
			std::error_code ec;
			result["size"] = fs::file_size(target, ec);
			return result;
		}, [run, target, keep, started](nlohmann::json result, const std::string& error) {
			if (!error.empty()) return run->Finish(false, "Backup failed: " + error);
			const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
			run->Log("Took " + std::to_string(static_cast<int>(seconds + 0.5)) + " s");
			if (result.contains("messages")) run->Log("mysqldump said: " + result["messages"].get<std::string>());
			const auto& check = result["check"];
			run->Log("Check: " + check.value("summary", ""));
			RememberCheck(target.filename().string(), check, "[backup]");
			if (check.contains("two_factor") && check["two_factor"].value("accounts", 0) > 0) {
				run->Log("Keep the two-factor key (dashboard_totp_key or totp_key) with this backup: " +
					std::to_string(check["two_factor"].value("accounts", 0)) + " account(s) need it to sign in with two-factor login");
			}
			// Keep the newest `keep`
			std::vector<std::string> names;
			for (const auto& entry : ListBackups()) names.push_back(entry.path().filename().string());
			uint32_t removed = 0;
			for (const auto& old : BackupFiles::Expired(names, keep)) {
				std::error_code ec;
				if (fs::remove(Folder() / old, ec)) { run->Log("Deleted old backup " + old); removed++; }
			}
			const auto megabytes = result.value("size", 0ull) / (1024.0 * 1024.0);
			char size[32];
			std::snprintf(size, sizeof(size), "%.1f MB", megabytes);
			BroadcastTableChanged("backups");
			run->Finish(true, target.filename().string() + " (" + size + ", checked)" + (removed ? ", deleted " + std::to_string(removed) + " old" : ""));
		});
		if (!queued) run->Finish(false, "A backup is already running");
	}
}

void RegisterBackupTask() {
	Scheduler::Register({ "database_backup", "Database backup",
		"Copies the database into backup_folder and checks the copy, keeping the newest backup_keep. SQLite is copied with "
		"VACUUM INTO while the servers run; MySQL uses mysqldump (backup_mysqldump). Off until you switch it on.",
		"30 3 * * *", RunBackup, 6 * 60 * 60, false });
}

void RegisterBackupRoutes() {
	Route(eHTTPMethod::GET, "/api/backups", Perm("backups"), "Database backups on the server, newest first, and where they go",
		[](HTTPReply& reply, const HTTPContext&) {
			nlohmann::json files = nlohmann::json::array();
			for (const auto& entry : ListBackups()) {
				std::error_code ec;
				const auto written = fs::last_write_time(entry.path(), ec);
				const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::clock_cast<std::chrono::system_clock>(written).time_since_epoch()).count();
				const auto name = entry.path().filename().string();
				nlohmann::json file = { {"name", name}, {"size", entry.file_size(ec)}, {"time", seconds},
					{"checking", Background::IsRunning("backup_verify:" + name)} };
				if (const auto check = g_Checks.find(name); check != g_Checks.end()) {
					file["check"] = check->second.result;
					file["checked_at"] = check->second.at;
					file["checked_by"] = check->second.by;
				}
				files.push_back(std::move(file));
			}
			JsonSuccess(reply, { {"folder", Folder().string()}, {"keep", GeneralUtils::TryParse<int64_t>(Game::config->GetValue("backup_keep")).value_or(7)},
				{"database", IsMySQL() ? "mysql" : "sqlite"}, {"files", files}, {"keep_with_backups", FilesToKeep()} });
		});

	Route(eHTTPMethod::POST, "/api/backups/:name/verify", Perm("backups"),
		"Check a backup can be restored: opens it read-only and runs PRAGMA integrity_check, counts every table's rows and checks the "
		"two-factor secrets decrypt with this server's key (SQLite), or checks the dump is complete (MySQL). Runs in the background; "
		"the result appears in GET /api/backups",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 2));
			if (!BackupFiles::IsName(name)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown backup");
			const auto file = Folder() / name;
			std::error_code ec;
			if (!fs::is_regular_file(file, ec)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Backup not found");
			const auto accountId = context.accountId;
			const auto user = context.authenticatedUser;
			const bool queued = Background::Run("backup_verify:" + name, [file](GameDatabase&) -> nlohmann::json {
				return VerifyFile(file, file.extension() == ".sql").ToJson();
			}, [name, accountId, user](nlohmann::json result, const std::string& error) {
				if (!error.empty()) result = { {"ok", false}, {"summary", "Check failed: " + error} };
				RememberCheck(name, result, user);
				Database::Get()->InsertAuditLog(accountId, user, "verify_backup_result", name + ": " + result.value("summary", ""), 0, 0);
				BroadcastTableChanged("backups");
			});
			if (!queued) return JsonError(reply, eHTTPStatusCode::CONFLICT, "That backup is already being checked");
			Audit(context, "verify_backup", name);
			BroadcastTableChanged("backups");
			JsonSuccess(reply, { {"message", "Checking " + name} });
		});

	Route(eHTTPMethod::POST, "/api/backups/run", Perm("backups"), "Make a backup now (runs the database_backup task)",
		[](HTTPReply& reply, const HTTPContext& context) {
			if (!Scheduler::RunNow("database_backup", context.authenticatedUser)) return JsonError(reply, eHTTPStatusCode::CONFLICT, "A backup is already running");
			Audit(context, "run_backup", "Started a database backup");
			JsonSuccess(reply, { {"message", "Backup started"} });
		});

	Route(eHTTPMethod::POST, "/api/backups/:name/delete", Perm("backups"), "Delete a backup",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 2));
			if (!BackupFiles::IsName(name)) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown backup");
			std::error_code ec;
			if (!fs::remove(Folder() / name, ec)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Backup not found");
			Audit(context, "delete_backup", name);
			JsonSuccess(reply, { {"message", "Deleted " + name} });
		});

	Route(eHTTPMethod::POST, "/api/backups/:name/link", Perm("backups"),
		"A one-time download link for a backup, valid for a minute. Body: {password, code (if you use two-factor login)}",
		[](HTTPReply& reply, const HTTPContext& context) {
			const std::string name(PathSegment(context.originalPath, 2));
			const auto body = ParseBody(context);
			if (!BackupFiles::IsName(name) || !body) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Unknown backup");
			std::error_code ec;
			if (!fs::is_regular_file(Folder() / name, ec)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Backup not found");
			// A backup holds every account's password hash and email, so ask for the password (and code) again
			const auto account = Database::Get()->GetAccountInfo(context.authenticatedUser);
			if (!account) return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "That password is not right");
			// Wrong passwords and codes here count like failed sign-ins, so a stolen session can't guess the password
			const auto address = ClientAddress(context);
			if (DashboardAuthService::IsThrottled(account->id, address)) {
				return JsonError(reply, eHTTPStatusCode::TOO_MANY_REQUESTS, "Too many wrong attempts, try again later");
			}
			const std::string password = body->value("password", "");
			if (password.empty() || password.size() > 40 || ::bcrypt_checkpw(password.c_str(), account->bcryptPassword.c_str()) != 0) {
				DashboardAuthService::RecordFailedAttempt(account->id, context.authenticatedUser, address);
				Audit(context, "backup_download_denied", name + ": wrong password");
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "That password is not right");
			}
			if (Database::Get()->GetTotp(account->id).enabledAt != 0 &&
				!DashboardAuthService::CheckTwoFactorCode(account->id, body->value("code", ""), false, nullptr)) {
				DashboardAuthService::RecordFailedAttempt(account->id, context.authenticatedUser, address);
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "Enter a current code from your authenticator app");
			}
			const auto now = static_cast<int64_t>(std::time(nullptr));
			std::erase_if(g_Links, [now](const auto& entry) { return entry.second.expires < now; });
			const auto token = GenerateUrlToken();
			g_Links[HashToken(token)] = { name, context.accountId, now + LINK_SECONDS };
			Audit(context, "backup_download", name);
			Alerts::Emit("security", "Database backup downloaded", context.authenticatedUser + " downloaded " + name + " from " + ClientAddress(context) + ".");
			JsonSuccess(reply, { {"url", "/api/backups/download?token=" + token} });
		});

	Route(eHTTPMethod::GET, "/api/backups/download", Perm("backups"), "Download a backup with a link from POST /api/backups/:name/link",
		[](HTTPReply& reply, const HTTPContext& context) {
			const auto token = QueryValue(context.queryString, "token");
			if (token.empty()) return JsonError(reply, eHTTPStatusCode::BAD_REQUEST, "Make a download link first (POST /api/backups/:name/link)");
			const auto it = g_Links.find(HashToken(token));
			const auto now = static_cast<int64_t>(std::time(nullptr));
			if (it == g_Links.end() || it->second.expires < now || it->second.accountId != context.accountId) {
				return JsonError(reply, eHTTPStatusCode::FORBIDDEN, "This download link has expired; make a new one");
			}
			const auto file = it->second.file;
			g_Links.erase(it); // one use
			std::error_code ec;
			if (!fs::is_regular_file(Folder() / file, ec)) return JsonError(reply, eHTTPStatusCode::NOT_FOUND, "Backup not found");
			reply.file = (Folder() / file).string(); // streamed from disk
			reply.message.clear();
			reply.status = eHTTPStatusCode::OK;
			reply.contentType = eContentType::APPLICATION_OCTET_STREAM;
			reply.headers.push_back("Content-Disposition: attachment; filename=\"" + file + "\"");
		});
}
