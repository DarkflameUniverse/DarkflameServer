#pragma once

#include "Database.h"

#include "dCommonVars.h"
#include "Game.h"
#include "dCommonVars.h"
#include "dLogger.h"

struct Migration {
	std::string data;
	std::string name;
};

class MigrationRunner {
public:
	static void RunMigrations();
	static Migration LoadMigration(std::string path);
};
