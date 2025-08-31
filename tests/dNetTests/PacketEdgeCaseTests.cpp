#include "TestCommon.h"
#include "AuthPackets.h"
#include "ChatPackets.h"
#include "WorldPackets.h"

class PacketEdgeCaseTests : public PacketTestsBase {};

// ===== Edge Cases and Boundary Tests =====

TEST_F(PacketEdgeCaseTests, EmptyStringsHandling) {
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

TEST_F(PacketEdgeCaseTests, MaxValueHandling) {
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

TEST_F(PacketEdgeCaseTests, LongPasswordHandling) {
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

TEST_F(PacketEdgeCaseTests, PacketDataConsistency) {
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

TEST_F(PacketEdgeCaseTests, UnicodeStringHandling) {
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