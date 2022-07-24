#include "Game.h"
#include "dZoneManager.h"

namespace Game {
	dLogger* logger;
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
		}

	EntityInfo info;
};
