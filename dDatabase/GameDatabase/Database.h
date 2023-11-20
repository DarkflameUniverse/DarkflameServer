#pragma once

#include <string>
#include <conncpp.hpp>

#include "GameDatabase.h"

namespace Database {
	void Connect();
	GameDatabase* Get();
	void Destroy(std::string source = "");
};
