#include "Game.h"

#include <random>

class Logger;
class dConfig;
namespace Game
{
	Logger* logger;
	dConfig* config = nullptr; // Permissions.cpp reads the permission levels from it
	std::mt19937 randomEngine;
} // namespace Game
