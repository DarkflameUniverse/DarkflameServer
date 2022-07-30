#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include <gtest/gtest.h>

class GameDependenciesTest : public ::testing::Test {
	protected:
		void SetUpDependencies() {
			info.pos = NiPoint3::ZERO;
			info.rot = NiQuaternion::IDENTITY;
			info.scale = 1.0f;
			info.spawner = nullptr;
			info.lot = 999;
			Game::logger = new dLogger("./testing.log", true, true);
			Game::server = new dServer("localhost", 3000, 0, 1, false, true, Game::logger, "localhost", 2000, ServerType::World, 1100);
		}

		void TearDownDependencies() {
			// TODO There are currently 9 extra memory defects due to Game::server not managing its memory properly.
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
