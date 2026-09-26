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
#include <vector>

class dZoneManager;
class AssetManager;

// A packet captured by dServerMock::Send. The bytes are copied, so the capture stays valid
// after the sender's local BitStream goes out of scope.
struct CapturedPacket {
	std::vector<uint8_t> bytes;
	uint32_t bits = 0;
	SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
	bool broadcast = false;
};

class dServerMock : public dServer {
	std::vector<CapturedPacket> sentPackets;
	RakNet::BitStream mostRecent;
public:
	dServerMock() {};
	~dServerMock() {};

	// Returns a copy of the most recently sent packet, with its read pointer at the start, or nullptr if nothing was sent.
	RakNet::BitStream* GetMostRecentBitStream() { return sentPackets.empty() ? nullptr : &mostRecent; };
	const std::vector<CapturedPacket>& GetSentPackets() const { return sentPackets; }
	void ClearSentPackets() { sentPackets.clear(); mostRecent.Reset(); }

	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override {
		CapturedPacket& packet = sentPackets.emplace_back();
		packet.bits = bitStream.GetNumberOfBitsUsed();
		packet.bytes.assign(bitStream.GetData(), bitStream.GetData() + bitStream.GetNumberOfBytesUsed());
		packet.sysAddr = sysAddr;
		packet.broadcast = broadcast;

		mostRecent.Reset();
		mostRecent.WriteBits(packet.bytes.data(), packet.bits, false);
	};
	void SetZoneId(unsigned int zoneId) { mZoneID = zoneId; }
};

class GameDependenciesTest : public ::testing::Test {
protected:
	void SetUpDependencies() {
		info.pos = NiPoint3Constant::ZERO;
		info.rot = QuatUtils::IDENTITY;
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
