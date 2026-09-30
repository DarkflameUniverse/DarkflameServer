#include "CDClientSnapshot.h"

#include <fstream>
#include <random>
#include <sstream>

#include "CppSQLite3.h"
#include "FdbToSqlite.h"
#include "GeneralUtils.h"
#include "MigrationRunner.h"

namespace {
	std::string ReadFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		std::stringstream text;
		text << file.rdbuf();
		return text.str();
	}

	// The same cdserver migrations RunSQLiteMigrations applies, into a fresh conversion: every one of them, since the
	// file never had any. Recorded in its migration_history like RunSQLiteMigrations does.
	void ApplyMigrations(CppSQLite3DB& db, const std::filesystem::path& folder, std::vector<std::string>& applied) {
		db.execDML("CREATE TABLE IF NOT EXISTS migration_history (name TEXT NOT NULL, date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP);");
		// RunSQLiteMigrations marks this one done on files it converts
		db.execDML("INSERT INTO migration_history (name) VALUES ('7_migration_for_migrations.sql');");
		std::error_code code;
		if (!std::filesystem::is_directory(folder, code)) return;

		for (const auto& name : GeneralUtils::GetSqlFileNamesFromFolder(folder.string())) {
			const auto sql = ReadFile(folder / name);
			if (sql.empty()) continue;
			db.execDML("BEGIN TRANSACTION;");
			for (const auto& statement : MigrationRunner::SplitStatements(sql, false)) {
				if (statement.empty()) continue;
				// A statement that fails is skipped, as RunSQLiteMigrations does
				try {
					db.execDML(statement.c_str());
				} catch (const CppSQLite3Exception&) {}
			}
			auto record = db.compileStatement("INSERT INTO migration_history (name) VALUES (?);");
			record.bind(1, name.c_str());
			record.execDML();
			record.finalize();
			db.execDML("COMMIT;");
			applied.push_back(name);
		}
	}
}

bool CDClientSnapshot::MakeSqlite(const std::filesystem::path& fdb, const std::filesystem::path& out, const std::filesystem::path& migrationsDir,
	std::vector<std::string>& migrations, std::string& error) {
	std::random_device random;
	const auto temp = out.parent_path() / (out.filename().string() + "." + FdbSnapshot::HashText((static_cast<uint64_t>(random()) << 32) ^ random()) + ".tmp");
	std::error_code code;

	std::ifstream input(fdb, std::ios::binary);
	if (!input) {
		error = "could not open " + fdb.string();
		return false;
	}

	try {
		CppSQLite3DB db;
		db.open(temp.string().c_str());
		FdbToSqlite::Convert convert(db);
		if (!convert.ConvertDatabase(input)) {
			error = "could not convert " + fdb.string() + ": " + convert.GetError();
			db.close();
			std::filesystem::remove(temp, code);
			return false;
		}
		ApplyMigrations(db, migrationsDir, migrations);
		db.close();
	} catch (const CppSQLite3Exception& e) {
		error = std::string("SQLite error making ") + out.string() + ": " + e.errorMessage();
		std::filesystem::remove(temp, code);
		return false;
	}

	std::filesystem::rename(temp, out, code);
	if (code) {
		error = "could not rename to " + out.string() + ": " + code.message();
		std::filesystem::remove(temp, code);
		return false;
	}
	return true;
}

CDClientSnapshot::Result CDClientSnapshot::Build(const std::filesystem::path& clientFdb, const std::filesystem::path& resServer,
	const std::filesystem::path& migrationsDir, std::optional<uint64_t> currentHash, const std::filesystem::path& previousFdb) {
	Result result;
	const auto hash = FdbSnapshot::MakeCopy(clientFdb, resServer, result.error);
	if (!hash) return result;
	result.hash = *hash;
	result.current.fdb = FdbSnapshot::FdbName(*hash);
	result.current.sqlite = FdbSnapshot::SqliteName(*hash);

	if (currentHash == hash) {
		result.ok = true;
		result.unchanged = true;
		return result;
	}

	std::error_code code;
	const auto sqlite = resServer / result.current.sqlite;
	// Made before for these bytes (the fdb went back to an older version); the file is complete, as it is renamed in
	if (!std::filesystem::exists(sqlite, code)) {
		if (!MakeSqlite(resServer / result.current.fdb, sqlite, migrationsDir, result.migrations, result.error)) return result;
	}

	if (!previousFdb.empty()) {
		result.changes = FdbSnapshot::DescribeChanges(FdbSnapshot::Summarize(previousFdb), FdbSnapshot::Summarize(resServer / result.current.fdb));
	}
	result.ok = true;
	return result;
}
