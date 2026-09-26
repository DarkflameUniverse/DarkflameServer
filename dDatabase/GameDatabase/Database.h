#pragma once

#include <memory>
#include <string>

#include "GameDatabase.h"

namespace Database {
	void Connect();
	GameDatabase* Get();
	void Destroy(std::string source = "");

	// A second, independent connection of the configured type, for work on another thread. Create it on the main
	// thread (connecting reads the config), then use it from one thread only.
	std::unique_ptr<GameDatabase> CreateConnection();

	// Used for assigning a test database as the handler for database logic.
	// Do not use in production code.
	void _setDatabase(GameDatabase* const db);

	std::string GetMigrationFolder();
};
