#include "TestCommon.h"
#include "WorldPackets.h"
#include "MessageType/Client.h"
#include "eRenameResponse.h"

class WorldPacketTests : public PacketTestsBase {};

// ===== WorldPackets Tests =====

TEST_F(WorldPacketTests, WorldHTTPMonitorInfoSerialization) {
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

TEST_F(WorldPacketTests, WorldCharacterDeleteResponse) {
	bool testResponse = false;
	
	WorldPackets::SendCharacterDeleteResponse(UNASSIGNED_SYSTEM_ADDRESS, testResponse);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::DELETE_CHARACTER_RESPONSE));
	
	uint8_t readResponse;
	bitStream->Read(readResponse);
	
	ASSERT_EQ(readResponse, static_cast<uint8_t>(testResponse));
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(WorldPacketTests, WorldCharacterRenameResponse) {
	// Test with different enum values for character rename response
	WorldPackets::SendCharacterRenameResponse(UNASSIGNED_SYSTEM_ADDRESS, eRenameResponse::NAME_IN_USE);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::CHARACTER_RENAME_RESPONSE));
	
	uint8_t readResponse;
	bitStream->Read(readResponse);
	
	ASSERT_EQ(readResponse, static_cast<uint8_t>(eRenameResponse::NAME_IN_USE));
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}

TEST_F(WorldPacketTests, WorldTransferToWorld) {
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

TEST_F(WorldPacketTests, WorldServerState) {
	WorldPackets::SendServerState(UNASSIGNED_SYSTEM_ADDRESS);
	
	auto* bitStream = GetMostRecentBitStream();
	VerifyHeader(bitStream, ServiceType::CLIENT, static_cast<uint32_t>(MessageType::Client::SERVER_STATES));
	
	uint8_t readState;
	bitStream->Read(readState);
	
	ASSERT_EQ(readState, 1); // Server is ready
	ASSERT_EQ(bitStream->GetNumberOfUnreadBits(), 0);
}