#include "GameDependencies.h"

namespace Game {
	dLogger* logger = nullptr;
	dServer* server = nullptr;
	dZoneManager* zoneManager = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	std::mt19937 randomEngine;
	RakPeerInterface* chatServer = nullptr;
	AssetManager* assetManager = nullptr;
	SystemAddress chatSysAddr;
	EntityManager* entityManager = nullptr;
}

void GameDependenciesTest::SetUpDependencies() {
	info.pos = NiPoint3::ZERO;
	info.rot = NiQuaternion::IDENTITY;
	info.scale = 1.0f;
	info.spawner = nullptr;
	info.lot = 999;
	Game::logger = new dLogger("./testing.log", true, true);
	Game::server = new dServerMock();
	Game::config = new dConfig("worldconfig.ini");
	Game::entityManager = new EntityManager();
	Game::zoneManager = new dZoneManager();
	Game::zoneManager->LoadZone(LWOZONEID(0, 0, 0));
}

void GameDependenciesTest::TearDownDependencies() {
	if (Game::server) delete Game::server;
	if (Game::entityManager) delete Game::entityManager;
	if (Game::config) delete Game::config;
	if (Game::zoneManager) delete Game::zoneManager;
	if (Game::logger) {
		Game::logger->Flush();
		delete Game::logger;
	}
}
