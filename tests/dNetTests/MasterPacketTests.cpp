#include "TestCommon.h"
#include "MasterPackets.h"
#include "MessageType/Master.h"

class MasterPacketTests : public PacketTestsBase {};

// ===== MasterPackets Tests =====
// Note: These tests verify packet structure creation, but cannot easily test SendToMaster 
// since it's not virtual. We can test the structure by calling the internal serialization.

TEST_F(MasterPacketTests, MasterPersistentIDResponseStructure) {
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

TEST_F(MasterPacketTests, MasterZoneTransferResponseStructure) {
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