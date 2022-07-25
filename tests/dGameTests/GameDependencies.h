#include "Game.h"
#include "dLogger.h"
#include "dZoneManager.h"
#include <gtest/gtest.h>

namespace Game {
	dLogger* logger = new dLogger("./testing.log", true, true);
	dServer* server;
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
		void SetUp() override {
			info.pos = NiPoint3::ZERO;
			info.rot = NiQuaternion::IDENTITY;
			info.scale = 1.0f;
			info.spawner = nullptr;
			info.lot = 999;
		}

	EntityInfo info;
};
