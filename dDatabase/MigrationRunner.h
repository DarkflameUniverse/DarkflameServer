#pragma once

#include <string>

struct Migration {
	std::string data;
	std::string name;
};

namespace MigrationRunner {
	void RunMigrations();
	void RunSQLiteMigrations();
	void MigrateLeaderboard();
};
