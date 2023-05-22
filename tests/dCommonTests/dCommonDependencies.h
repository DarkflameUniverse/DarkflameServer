#ifndef __DCOMMONDEPENDENCIES__H__
#define __DCOMMONDEPENDENCIES__H__

#include "Game.h"
#include "Logger.h"
#include "dServer.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dConfig.h"
#include <gtest/gtest.h>

class dCommonDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		Game::logger = std::make_unique<Logger>("./testing.log", true, true);
	}

	void TearDownDependencies() { }
};

#endif  //!__DCOMMONDEPENDENCIES__H__
