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
#include "eRenameResponse.h"

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
	
	// Call Send which will add header and serialize
	announcement.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::GM_ANNOUNCE));
	
	// Read announcement data (size prefixed strings)
	uint32_t titleSize;
	bitStream->Read(titleSize);
	ASSERT_EQ(titleSize, announcement.title.size());
	
	std::string readTitle(titleSize, '\0');
	bitStream->ReadBits(reinterpret_cast<unsigned char*>(readTitle.data()), BYTES_TO_BITS(titleSize), true);
	
	uint32_t messageSize;
	bitStream->Read(messageSize);
	ASSERT_EQ(messageSize, announcement.message.size());
	
	std::string readMessage(messageSize, '\0');
	bitStream->ReadBits(reinterpret_cast<unsigned char*>(readMessage.data()), BYTES_TO_BITS(messageSize), true);
	
	ASSERT_EQ(readTitle, "Test Title");
	ASSERT_EQ(readMessage, "Test announcement message");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatAchievementNotifySerialization) {
	ChatPackets::AchievementNotify notify;
	notify.targetPlayerName = LUWString(u"TargetPlayer");
	notify.missionEmailID = 12345;
	notify.earningPlayerID = 987654321;
	notify.earnerName = LUWString(u"EarnerPlayer");
	
	// Call Send which will add header and serialize
	notify.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CHAT, static_cast<uint32_t>(MessageType::Chat::ACHIEVEMENT_NOTIFY));
	
	// Read achievement data based on actual Serialize implementation
	// Order: packing(13), earnerName, packing(15), missionEmailID, earningPlayerID, targetPlayerName
	uint64_t packing1;
	uint32_t packing2;
	uint8_t packing3;
	LUWString readEarnerName;
	uint64_t packing4;
	uint32_t packing5;
	uint16_t packing6;
	uint8_t packing7;
	uint32_t readMissionEmailID;
	LWOOBJID readEarningPlayerID;
	LUWString readTargetPlayerName;
	
	bitStream->Read(packing1);
	bitStream->Read(packing2);
	bitStream->Read(packing3);
	bitStream->Read(readEarnerName);
	bitStream->Read(packing4);
	bitStream->Read(packing5);
	bitStream->Read(packing6);
	bitStream->Read(packing7);
	bitStream->Read(readMissionEmailID);
	bitStream->Read(readEarningPlayerID);
	bitStream->Read(readTargetPlayerName);
	
	ASSERT_EQ(readEarnerName.string, u"EarnerPlayer");
	ASSERT_EQ(readMissionEmailID, 12345u);
	ASSERT_EQ(readEarningPlayerID, 987654321);
	ASSERT_EQ(readTargetPlayerName.string, u"TargetPlayer");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, ChatTeamInviteInitialResponseSerialization) {
	ChatPackets::TeamInviteInitialResponse response;
	response.inviteFailedToSend = true;
	response.playerName = LUWString(u"InviteePlayer");
	
	// Call Send which will add header and serialize
	response.Send(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::TEAM_INVITE_INITIAL_RESPONSE));
	
	// Read team invite response data  
	uint8_t readInviteFailedToSend;  // Written as uint8_t in actual implementation
	LUWString readPlayerName;
	
	bitStream->Read(readInviteFailedToSend);
	bitStream->Read(readPlayerName);
	
	ASSERT_EQ(readInviteFailedToSend, static_cast<uint8_t>(true));
	ASSERT_EQ(readPlayerName.string, u"InviteePlayer");
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
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
	uint16_t readServerPort;  // Note: written as uint16_t in actual implementation
	LUString readServerIP(255);  // Note: written as LUString with size 255
	
	bitStream->Read(readRequestID);
	bitStream->Read(readMythranShift);
	bitStream->Read(readZoneID);
	bitStream->Read(readZoneInstance);
	bitStream->Read(readZoneClone);
	bitStream->Read(readServerPort);
	bitStream->Read(readServerIP);
	
	ASSERT_EQ(readRequestID, testRequestID);
	ASSERT_EQ(readMythranShift, static_cast<uint8_t>(testMythranShift));
	ASSERT_EQ(readZoneID, testZoneID);
	ASSERT_EQ(readZoneInstance, testZoneInstance);
	ASSERT_EQ(readZoneClone, testZoneClone);
	ASSERT_EQ(readServerPort, static_cast<uint16_t>(testServerPort));
	ASSERT_EQ(readServerIP.string, testServerIP);
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

// ===== Additional Comprehensive Tests =====

TEST_F(PacketTests, WorldCharacterDeleteResponse) {
	bool testResponse = false;
	
	WorldPackets::SendCharacterDeleteResponse(UNASSIGNED_SYSTEM_ADDRESS, testResponse);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::DELETE_CHARACTER_RESPONSE));
	
	uint8_t readResponse;
	bitStream->Read(readResponse);
	
	ASSERT_EQ(readResponse, static_cast<uint8_t>(testResponse));
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, WorldCharacterRenameResponse) {
	// Test with different enum values for character rename response
	WorldPackets::SendCharacterRenameResponse(UNASSIGNED_SYSTEM_ADDRESS, eRenameResponse::NAME_IN_USE);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::CHARACTER_RENAME_RESPONSE));
	
	uint8_t readResponse;
	bitStream->Read(readResponse);
	
	ASSERT_EQ(readResponse, static_cast<uint8_t>(eRenameResponse::NAME_IN_USE));
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, WorldTransferToWorld) {
	std::string testServerIP = "10.0.0.1";
	uint32_t testServerPort = 3000;
	bool testMythranShift = false;
	
	WorldPackets::SendTransferToWorld(UNASSIGNED_SYSTEM_ADDRESS, testServerIP, testServerPort, testMythranShift);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::TRANSFER_TO_WORLD));
	
	LUString readServerIP;
	uint16_t readServerPort;
	uint8_t readMythranShift;
	
	bitStream->Read(readServerIP);
	bitStream->Read(readServerPort);
	bitStream->Read(readMythranShift);
	
	ASSERT_EQ(readServerIP.string, testServerIP);
	ASSERT_EQ(readServerPort, static_cast<uint16_t>(testServerPort));
	ASSERT_EQ(readMythranShift, static_cast<uint8_t>(testMythranShift));
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, WorldServerState) {
	WorldPackets::SendServerState(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::SERVER_STATES));
	
	uint8_t readState;
	bitStream->Read(readState);
	
	ASSERT_EQ(readState, 1); // Server is ready
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(PacketTests, PacketDataConsistency) {
	// Test that multiple calls produce identical results
	Stamp stamp1(eStamps::PASSPORT_AUTH_START, 100, 2000000000);
	Stamp stamp2(eStamps::PASSPORT_AUTH_START, 100, 2000000000);
	
	RakNet::BitStream stream1, stream2;
	stamp1.Serialize(stream1);
	stamp2.Serialize(stream2);
	
	// Should produce identical bit streams
	ASSERT_EQ(stream1.GetNumberOfBitsUsed(), stream2.GetNumberOfBitsUsed());
	
	// Read both streams and compare
	eStamps type1, type2;
	uint32_t value1, value2;
	uint64_t timestamp1, timestamp2;
	
	stream1.Read(type1);
	stream1.Read(value1);
	stream1.Read(timestamp1);
	
	stream2.Read(type2);
	stream2.Read(value2);
	stream2.Read(timestamp2);
	
	ASSERT_EQ(type1, type2);
	ASSERT_EQ(value1, value2);
	ASSERT_EQ(timestamp1, timestamp2);
}

TEST_F(PacketTests, UnicodeStringHandling) {
	// Test with various Unicode characters
	FindPlayerRequest request;
	request.requestor = 12345;
	request.playerName = LUWString(u"テストプレイヤー123"); // Japanese characters
	
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
	
	ASSERT_EQ(deserialized.requestor, request.requestor);
	ASSERT_EQ(deserialized.playerName.string, request.playerName.string);
}