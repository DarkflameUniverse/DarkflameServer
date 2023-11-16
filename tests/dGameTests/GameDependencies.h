#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "Logger.h"
#include "dServer.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dConfig.h"
#include <gtest/gtest.h>

class dZoneManager;
class AssetManager;

class dServerMock : public dServer {
	RakNet::BitStream* sentBitStream = nullptr;
public:
	dServerMock() {};
	~dServerMock() {};
	RakNet::BitStream* GetMostRecentBitStream() { return sentBitStream; };
	void Send(RakNet::BitStream* bitStream, const SystemAddress& sysAddr, bool broadcast) override { sentBitStream = bitStream; };
};

class GameDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		info.pos = NiPoint3::ZERO;
		info.rot = NiQuaternion::IDENTITY;
		info.scale = 1.0f;
		info.spawner = nullptr;
		info.lot = 999;
		Game::logger = new Logger("./testing.log", true, true);
		Game::server = new dServerMock();
		Game::config = new dConfig("worldconfig.ini");
		Game::entityManager = new EntityManager();
	}

	void TearDownDependencies() {
		if (Game::server) delete Game::server;
		if (Game::entityManager) delete Game::entityManager;
		if (Game::logger) {
			Game::logger->Flush();
			delete Game::logger;
		}
		if (Game::config) delete Game::config;
	}

	EntityInfo info{};
};

#endif //!__GAMEDEPENDENCIES__H__
