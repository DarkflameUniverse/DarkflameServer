#pragma once

#include <string>

#include "MigrationManager.h"

class MySQLMigrationManager : public MigrationManager {
public:
	void RunMigrations(DatabaseBase* db) override;
	void RunSQLiteMigrations() override;
private:

};
