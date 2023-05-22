#include "Game.h"

#include "Logger.h"
#include <memory>

class Logger;
namespace Game
{
	std::unique_ptr<Logger> logger = std::make_unique<Logger>("./testing.log", true, true);
} // namespace Game
