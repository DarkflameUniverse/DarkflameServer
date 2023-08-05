#ifndef __GAMEDEPENDENCIES__H__
#define __GAMEDEPENDENCIES__H__

#include "Game.h"
#include "dLogger.h"
#include "dServer.h"
#include "EntityInfo.h"
#include "EntityManager.h"
#include "dZoneManager.h"
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
	void SetUpDependencies();

	void TearDownDependencies();

	EntityInfo info;
};

#endif //!__GAMEDEPENDENCIES__H__
