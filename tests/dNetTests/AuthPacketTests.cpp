#include "TestCommon.h"
#include "AuthPackets.h"
#include "MessageType/Auth.h"

class AuthPacketTests : public PacketTestsBase {};

// ===== AuthPackets Tests =====

TEST_F(AuthPacketTests, AuthStampSerialization) {
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