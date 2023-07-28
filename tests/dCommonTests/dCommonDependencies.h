#ifndef __DCOMMONDEPENDENCIES__H__
#define __DCOMMONDEPENDENCIES__H__

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dConfig.h"
#include <gtest/gtest.h>

class dCommonDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		Game::logger = new dLogger("./testing.log", true, true);
	}

	void TearDownDependencies() {
		if (Game::logger) {
			Game::logger->Flush();
			delete Game::logger;
		}
	}
};

#endif  //!__DCOMMONDEPENDENCIES__H__
