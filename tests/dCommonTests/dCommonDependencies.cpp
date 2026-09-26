#include "Game.h"

class Logger;
class dConfig;
namespace Game
{
	Logger* logger;
	dConfig* config = nullptr; // Permissions.cpp reads the permission levels from it
} // namespace Game
