#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include <gtest/gtest.h>

class dServerMock : public dServer {
public:
	dServerMock() {};
	~dServerMock() {};
	void Send(RakNet::BitStream* bitStream, const SystemAddress& sysAddr, bool broadcast) override {};
};

class GameDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		info.pos = NiPoint3::ZERO;
		info.rot = NiQuaternion::IDENTITY;
		info.scale = 1.0f;
		info.spawner = nullptr;
		info.lot = 999;
		Game::logger = new dLogger("./testing.log", true, true);
		Game::server = new dServerMock();
	}

	void TearDownDependencies() {
		if (Game::server) delete Game::server;
		delete EntityManager::Instance();
		delete dZoneManager::Instance();
		if (Game::logger) {
			Game::logger->Flush();
			delete Game::logger;
		}
	}

	EntityInfo info;
};

#endif //!__GAMEDEPENDENCIES__H__
