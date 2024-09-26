#pragma once

#include <string>
#include <conncpp.hpp>

#include "GameDatabase.h"

namespace Database {
	void Connect();
	GameDatabase* Get();
	void Destroy(std::string source = "");

	// Used for assigning a test database as the handler for database logic.
	// Do not use in production code.
	void _setDatabase(GameDatabase* const db);
};
