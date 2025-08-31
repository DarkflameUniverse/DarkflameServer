#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "Logger.h"
#include "dServer.h"
#include "CDClientManager.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dConfig.h"
#include "dZoneManager.h"
#include "GameDatabase/TestSQL/TestSQLDatabase.h"
#include "Database.h"
#include <gtest/gtest.h>

class dZoneManager;
class AssetManager;

class dServerMock : public dServer {
	RakNet::BitStream* sentBitStream = nullptr;
public:
	dServerMock() {};
	~dServerMock() {};
	RakNet::BitStream* GetMostRecentBitStream() { return sentBitStream; };
	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override { sentBitStream = &bitStream; };
	void SetZoneId(unsigned int zoneId) { mZoneID = zoneId; }
};

class GameDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		info.pos = NiPoint3Constant::ZERO;
		info.rot = NiQuaternionConstant::IDENTITY;
		info.scale = 1.0f;
		info.spawner = nullptr;
		info.lot = 999;
		Game::logger = new Logger("./testing.log", true, true);
		Game::server = new dServerMock();
		Game::config = new dConfig("worldconfig.ini");
		Game::entityManager = new EntityManager();
		Game::zoneManager = new dZoneManager();
		Game::zoneManager->LoadZone(LWOZONEID(1, 0, 0));
		Database::_setDatabase(new TestSQLDatabase()); // this new is managed by the Database

		// Create a CDClientManager instance and load from defaults
		CDClientManager::LoadValuesFromDefaults();
	}

	void TearDownDependencies() {
		if (Game::server) delete Game::server;
		if (Game::entityManager) delete Game::entityManager;
		if (Game::zoneManager) delete Game::zoneManager;
		if (Game::logger) {
			Game::logger->Flush();
			delete Game::logger;
		}
		if (Game::config) delete Game::config;
	}

	EntityInfo info{};
};

#endif //!__GAMEDEPENDENCIES__H__
