#include <gtest/gtest.h>
#include <memory>

#include "BitStream.h"
#include "dCommonVars.h"
#include "RakNetTypes.h"

// Packet headers
#include "AuthPackets.h"
#include "ChatPackets.h" 
#include "ClientPackets.h"
#include "WorldPackets.h"
#include "MasterPackets.h"
#include "BitStreamUtils.h"
#include "ServiceType.h"
#include "MessageType/Auth.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Master.h"

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
	Logger* logger = nullptr;
	dServer* server = nullptr;
	dZoneManager* zoneManager = nullptr;
	dChatFilter* chatFilter = nullptr;
	dConfig* config = nullptr;
	std::mt19937 randomEngine;
	RakPeerInterface* chatServer = nullptr;
	AssetManager* assetManager = nullptr;
	SystemAddress chatSysAddr;
	EntityManager* entityManager = nullptr;
	std::string projectVersion;
	signal_t lastSignal = 0;
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

class PacketTests : public ::testing::Test {
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

// ===== AuthPackets Tests =====

TEST_F(PacketTests, AuthStampSerialization) {
	Stamp testStamp(eStamps::PASSPORT_AUTH_START, 12345, 1609459200); // Jan 1, 2021 timestamp
	
	RakNet::BitStream bitStream;
	testStamp.Serialize(bitStream);
	
	// Verify serialized data
	eStamps readType;
	uint32_t readValue;
	uint64_t readTimestamp;
	
	bitStream.Read(readType);
	bitStream.Read(readValue);
	bitStream.Read(readTimestamp);
	
	ASSERT_EQ(readType, eStamps::PASSPORT_AUTH_START);
	ASSERT_EQ(readValue, 12345u);
	ASSERT_EQ(readTimestamp, 1609459200u);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

// ===== ChatPackets Tests =====

TEST_F(PacketTests, ChatShowAllRequestSerialization) {
	ShowAllRequest request;
	request.requestor = 123456789;
	request.displayZoneData = true;
	request.displayIndividualPlayers = false;
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::SHOW_ALL));
	
	LWOOBJID readRequestor;
	bool readDisplayZoneData;
	bool readDisplayIndividualPlayers;
	
	bitStream.Read(readRequestor);
	bitStream.Read(readDisplayZoneData);
	bitStream.Read(readDisplayIndividualPlayers);
	
	ASSERT_EQ(readRequestor, 123456789);
	ASSERT_EQ(readDisplayZoneData, true);
	ASSERT_EQ(readDisplayIndividualPlayers, false);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatShowAllRequestRoundTrip) {
	ShowAllRequest original;
	original.requestor = 987654321;
	original.displayZoneData = false;
	original.displayIndividualPlayers = true;
	
	RakNet::BitStream bitStream;
	original.Serialize(bitStream);
	
	// Skip header for deserialization
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	ShowAllRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, original.requestor);
	ASSERT_EQ(deserialized.displayZoneData, original.displayZoneData);
	ASSERT_EQ(deserialized.displayIndividualPlayers, original.displayIndividualPlayers);
}

TEST_F(PacketTests, ChatFindPlayerRequestSerialization) {
	FindPlayerRequest request;
	request.requestor = 555555555;
	request.playerName = LUWString(u"TestPlayer123");
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::WHO));
	
	LWOOBJID readRequestor;
	LUWString readPlayerName;
	
	bitStream.Read(readRequestor);
	bitStream.Read(readPlayerName);
	
	ASSERT_EQ(readRequestor, 555555555);
	ASSERT_EQ(readPlayerName.string, u"TestPlayer123");
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatFindPlayerRequestRoundTrip) {
	FindPlayerRequest original;
	original.requestor = 777777777;
	original.playerName = LUWString(u"PlayerToFind");
	
	RakNet::BitStream bitStream;
	original.Serialize(bitStream);
	
	// Skip header for deserialization
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	FindPlayerRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, original.requestor);
	ASSERT_EQ(deserialized.playerName.string, original.playerName.string);
}

TEST_F(PacketTests, ChatAnnouncementSerialization) {
	ChatPackets::Announcement announcement;
	announcement.title = "Test Title";
	announcement.message = "Test announcement message";
	
	RakNet::BitStream bitStream;
	announcement.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::GM_ANNOUNCE));
	
	// Read announcement data
	LUString readTitle;
	LUString readMessage;
	
	bitStream.Read(readTitle);
	bitStream.Read(readMessage);
	
	ASSERT_EQ(readTitle.string, "Test Title");
	ASSERT_EQ(readMessage.string, "Test announcement message");
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatAchievementNotifySerialization) {
	ChatPackets::AchievementNotify notify;
	notify.targetPlayerName = LUWString(u"TargetPlayer");
	notify.missionEmailID = 12345;
	notify.earningPlayerID = 987654321;
	notify.earnerName = LUWString(u"EarnerPlayer");
	
	RakNet::BitStream bitStream;
	notify.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::ACHIEVEMENT_NOTIFY));
	
	// Read achievement data
	LUWString readTargetPlayerName;
	uint32_t readMissionEmailID;
	LWOOBJID readEarningPlayerID;
	LUWString readEarnerName;
	
	bitStream.Read(readTargetPlayerName);
	bitStream.Read(readMissionEmailID);
	bitStream.Read(readEarningPlayerID);
	bitStream.Read(readEarnerName);
	
	ASSERT_EQ(readTargetPlayerName.string, u"TargetPlayer");
	ASSERT_EQ(readMissionEmailID, 12345u);
	ASSERT_EQ(readEarningPlayerID, 987654321);
	ASSERT_EQ(readEarnerName.string, u"EarnerPlayer");
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatTeamInviteInitialResponseSerialization) {
	ChatPackets::TeamInviteInitialResponse response;
	response.inviteFailedToSend = true;
	response.playerName = LUWString(u"InviteePlayer");
	
	RakNet::BitStream bitStream;
	response.Serialize(bitStream);
	
	VerifyHeader(&bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE));
	
	// Read team invite response data
	bool readInviteFailedToSend;
	LUWString readPlayerName;
	
	bitStream.Read(readInviteFailedToSend);
	bitStream.Read(readPlayerName);
	
	ASSERT_EQ(readInviteFailedToSend, true);
	ASSERT_EQ(readPlayerName.string, u"InviteePlayer");
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

// ===== WorldPackets Tests =====

TEST_F(PacketTests, WorldHTTPMonitorInfoSerialization) {
	HTTPMonitorInfo info;
	info.port = 8080;
	info.openWeb = true;
	info.supportsSum = false;
	info.supportsDetail = true;
	info.supportsWho = false;
	info.supportsObjects = true;
	
	RakNet::BitStream bitStream;
	info.Serialize(bitStream);
	
	// Read serialized data
	uint16_t readPort;
	uint8_t readOpenWeb;
	uint8_t readSupportsSum;
	uint8_t readSupportsDetail;
	uint8_t readSupportsWho;
	uint8_t readSupportsObjects;
	
	bitStream.Read(readPort);
	bitStream.Read(readOpenWeb);
	bitStream.Read(readSupportsSum);
	bitStream.Read(readSupportsDetail);
	bitStream.Read(readSupportsWho);
	bitStream.Read(readSupportsObjects);
	
	ASSERT_EQ(readPort, 8080);
	ASSERT_EQ(readOpenWeb, 1);
	ASSERT_EQ(readSupportsSum, 0);
	ASSERT_EQ(readSupportsDetail, 1);
	ASSERT_EQ(readSupportsWho, 0);
	ASSERT_EQ(readSupportsObjects, 1);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}

// ===== MasterPackets Tests =====
// Note: These tests verify packet structure creation, but cannot easily test SendToMaster 
// since it's not virtual. We can test the structure by calling the internal serialization.

TEST_F(PacketTests, MasterPersistentIDResponseStructure) {
	uint64_t testRequestID = 9876543210987654321ULL;
	uint32_t testObjID = 555555;
	
	MasterPackets::SendPersistentIDResponse(Game::server, UNASSIGNED_SYSTEM_ADDRESS, testRequestID, testObjID);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::MASTER, static_cast<uint32_t>(MessageType::Master::REQUEST_PERSISTENT_ID_RESPONSE));
	
	uint64_t readRequestID;
	uint32_t readObjID;
	
	bitStream->Read(readRequestID);
	bitStream->Read(readObjID);
	
	ASSERT_EQ(readRequestID, testRequestID);
	ASSERT_EQ(readObjID, testObjID);
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, MasterZoneTransferResponseStructure) {
	uint64_t testRequestID = 1111111111111111111ULL;
	bool testMythranShift = true;
	uint32_t testZoneID = 1000;
	uint32_t testZoneInstance = 5;
	uint32_t testZoneClone = 10;
	std::string testServerIP = "192.168.1.100";
	uint32_t testServerPort = 2002;
	
	MasterPackets::SendZoneTransferResponse(Game::server, UNASSIGNED_SYSTEM_ADDRESS, testRequestID, testMythranShift, testZoneID, testZoneInstance, testZoneClone, testServerIP, testServerPort);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::MASTER, static_cast<uint32_t>(MessageType::Master::REQUEST_ZONE_TRANSFER_RESPONSE));
	
	uint64_t readRequestID;
	uint8_t readMythranShift;
	uint32_t readZoneID;
	uint32_t readZoneInstance;
	uint32_t readZoneClone;
	LUString readServerIP;
	uint32_t readServerPort;
	
	bitStream->Read(readRequestID);
	bitStream->Read(readMythranShift);
	bitStream->Read(readZoneID);
	bitStream->Read(readZoneInstance);
	bitStream->Read(readZoneClone);
	bitStream->Read(readServerIP);
	bitStream->Read(readServerPort);
	
	ASSERT_EQ(readRequestID, testRequestID);
	ASSERT_EQ(readMythranShift, static_cast<uint8_t>(testMythranShift));
	ASSERT_EQ(readZoneID, testZoneID);
	ASSERT_EQ(readZoneInstance, testZoneInstance);
	ASSERT_EQ(readZoneClone, testZoneClone);
	ASSERT_EQ(readServerIP.string, testServerIP);
	ASSERT_EQ(readServerPort, testServerPort);
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

// ===== Edge Cases and Boundary Tests =====

TEST_F(PacketTests, EmptyStringsHandling) {
	FindPlayerRequest request;
	request.requestor = 0;
	request.playerName = LUWString(u"");
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	// Skip header
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	FindPlayerRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, 0);
	ASSERT_EQ(deserialized.playerName.string, u"");
}

TEST_F(PacketTests, MaxValueHandling) {
	// Test with maximum values
	ShowAllRequest request;
	request.requestor = LWOOBJID_EMPTY;  // Max LWOOBJID
	request.displayZoneData = true;
	request.displayIndividualPlayers = true;
	
	RakNet::BitStream bitStream;
	request.Serialize(bitStream);
	
	// Skip header
	uint8_t rakNetPacketId{};
	uint16_t serviceType{};
	uint32_t packetId{};
	uint8_t always0{};
	bitStream.Read(rakNetPacketId);
	bitStream.Read(serviceType);
	bitStream.Read(packetId);
	bitStream.Read(always0);
	
	ShowAllRequest deserialized;
	deserialized.Deserialize(bitStream);
	
	ASSERT_EQ(deserialized.requestor, LWOOBJID_EMPTY);
	ASSERT_EQ(deserialized.displayZoneData, true);
	ASSERT_EQ(deserialized.displayIndividualPlayers, true);
}

TEST_F(PacketTests, LongPasswordHandling) {
	// Test with a very long password for HTTPMonitorInfo (which has port field)
	HTTPMonitorInfo info;
	info.port = 65535; // Max port value
	info.openWeb = true;
	info.supportsSum = true;
	info.supportsDetail = true;
	info.supportsWho = true;
	info.supportsObjects = true;
	
	RakNet::BitStream bitStream;
	info.Serialize(bitStream);
	
	// Read serialized data
	uint16_t readPort;
	uint8_t readOpenWeb;
	uint8_t readSupportsSum;
	uint8_t readSupportsDetail;
	uint8_t readSupportsWho;
	uint8_t readSupportsObjects;
	
	bitStream.Read(readPort);
	bitStream.Read(readOpenWeb);
	bitStream.Read(readSupportsSum);
	bitStream.Read(readSupportsDetail);
	bitStream.Read(readSupportsWho);
	bitStream.Read(readSupportsObjects);
	
	ASSERT_EQ(readPort, 65535);
	ASSERT_EQ(readOpenWeb, 1);
	ASSERT_EQ(readSupportsSum, 1);
	ASSERT_EQ(readSupportsDetail, 1);
	ASSERT_EQ(readSupportsWho, 1);
	ASSERT_EQ(readSupportsObjects, 1);
	ASSERT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
}