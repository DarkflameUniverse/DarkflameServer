#pragma once

#include <gtest/gtest.h>
#include <memory>

#include "BitStream.h"
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "BitStreamUtils.h"
#include "ServiceType.h"

// Game dependencies for mocking
#include "Game.h"
#include "dServer.h"

// Forward declarations to minimize dependencies
class Logger;
class dConfig;
class AssetManager;
class dZoneManager;
class dChatFilter;
class EntityManager;

// Define Game namespace globals needed for linking
namespace Game {
	extern Logger* logger;
	extern dServer* server;
	extern dZoneManager* zoneManager;
	extern dChatFilter* chatFilter;
	extern dConfig* config;
	extern std::mt19937 randomEngine;
	extern RakPeerInterface* chatServer;
	extern AssetManager* assetManager;
	extern SystemAddress chatSysAddr;
	extern EntityManager* entityManager;
	extern std::string projectVersion;
	extern signal_t lastSignal;
}

// Simple mock server that captures bit streams for testing
class dServerMock : public dServer {
public:
	dServerMock() {}
	
	RakNet::BitStream* GetMostRecentBitStream() {
		return this->m_MostRecentBitStream.get();
	}

	void Send(RakNet::BitStream& bitStream, const SystemAddress& sysAddr, bool broadcast) override {
		this->m_MostRecentBitStream = std::make_unique<RakNet::BitStream>();
		*this->m_MostRecentBitStream = bitStream;
	}

private:
	std::unique_ptr<RakNet::BitStream> m_MostRecentBitStream = nullptr;
};

class PacketTestsBase : public ::testing::Test {
protected:
	void SetUp() override {
		// Set up a minimal mock server for testing
		Game::server = new dServerMock();
	}

	void TearDown() override {
		if (Game::server) {
			delete Game::server;
			Game::server = nullptr;
		}
	}

	/**
	 * Helper to get the most recent bit stream from the mock server
	 */
	RakNet::BitStream* GetMostRecentBitStream() {
		auto* server = static_cast<dServerMock*>(Game::server);
		return server->GetMostRecentBitStream();
	}

	/**
	 * Helper to verify packet header
	 */
	void VerifyHeader(RakNet::BitStream* bitStream, ServiceType expectedServiceType, uint32_t expectedPacketId) {
		ASSERT_NE(bitStream, nullptr);
		
		uint8_t rakNetPacketId{};
		uint16_t serviceType{};
		uint32_t packetId{};
		uint8_t always0{};

		bitStream->Read(rakNetPacketId);
		bitStream->Read(serviceType);
		bitStream->Read(packetId);
		bitStream->Read(always0);

		ASSERT_EQ(rakNetPacketId, 0x53); // ID_USER_PACKET_ENUM
		ASSERT_EQ(serviceType, static_cast<uint16_t>(expectedServiceType));
		ASSERT_EQ(packetId, expectedPacketId);
		ASSERT_EQ(always0, 0x00);
	}
};