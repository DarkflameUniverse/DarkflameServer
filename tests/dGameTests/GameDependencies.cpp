#include "GameDependencies.h"

namespace Game {
	dLogger* logger = new dLogger("./testing.log", true, true);
	dServer* server = new dServer("localhost", 3000, 0, 1, false, true, Game::logger, "localhost", 2000, ServerType::World, 1100);
	dZoneManager* zoneManager;
	dpWorld* physicsWorld;
	dChatFilter* chatFilter;
	dConfig* config;
	dLocale* locale;
	std::mt19937 randomEngine;
	RakPeerInterface* chatServer;
	SystemAddress chatSysAddr;
}
