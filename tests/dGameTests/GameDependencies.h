#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "dZoneManager.h"
#include <gtest/gtest.h>

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

class GameDependenciesTest : public ::testing::Test {
	protected:
		void SetUpDependencies() {
			info.pos = NiPoint3::ZERO;
			info.rot = NiQuaternion::IDENTITY;
			info.scale = 1.0f;
			info.spawner = nullptr;
			info.lot = 999;
		}

	EntityInfo info;
};
