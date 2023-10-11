#pragma once

class DatabaseBase;

struct Migration {
	std::string data;
	std::string name;

	static Migration LoadMigration(const std::string& type, const std::string& dbType, const std::string& name);
};

class MigrationManager {
public:
	void RunMigrations(DatabaseBase* db);
	void RunSQLiteMigrations();
private:
};
