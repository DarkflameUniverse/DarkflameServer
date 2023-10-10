#pragma once

#include <string>

#include "Databases/DatabaseBase.h"

class dConfig;

class Database {
public:
	static DatabaseBase* Connection;
	static eConnectionTypes ConnectionType;

	static void Connect(dConfig* config);
	static void Destroy();
};
