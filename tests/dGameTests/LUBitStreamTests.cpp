#include <gtest/gtest.h>

#include "BitStreamUtils.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"

#include <vector>

namespace {
	std::vector<uint8_t> Bytes(RakNet::BitStream& bitStream) {
		return { bitStream.GetData(), bitStream.GetData() + bitStream.GetNumberOfBytesUsed() };
	}
}

namespace {
	struct TestPacket : public LUBitStream {
		uint32_t value = 0;
		TestPacket() : LUBitStream(ServiceType::CHAT, 0x1234u) {}
		void Serialize(RakNet::BitStream& bitStream) const override { bitStream.Write(value); }
		bool Deserialize(RakNet::BitStream& bitStream) override { return bitStream.Read(value); }
	};
}

TEST(LUBitStreamStructTests, WritePacketGoldenAndRoundTrip) {
	TestPacket packet;
	packet.value = 0xAABBCCDD;
	RakNet::BitStream bitStream;
	packet.WritePacket(bitStream);
	// 0x53 | service u16 | packet id u32 | pad u8 | payload
	const std::vector<uint8_t> golden = { 0x53, 0x02, 0x00, 0x34, 0x12, 0x00, 0x00, 0x00, 0xDD, 0xCC, 0xBB, 0xAA };
	ASSERT_EQ(Bytes(bitStream), golden);

	TestPacket read;
	ASSERT_TRUE(read.ReadHeader(bitStream));
	EXPECT_EQ(read.connectionType, ServiceType::CHAT);
	EXPECT_EQ(read.internalPacketID, 0x1234u);
	ASSERT_TRUE(read.Deserialize(bitStream));
	EXPECT_EQ(read.value, 0xAABBCCDD);
}

class LUBitStreamTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(LUBitStreamTests, SendWritesWhatWritePacketWrites) {
	TestPacket packet;
	packet.value = 7;
	RakNet::BitStream expected;
	packet.WritePacket(expected);
	const auto sent = PacketTestUtils::Capture([&] { packet.Send(UNASSIGNED_SYSTEM_ADDRESS); });
	ASSERT_EQ(sent.size(), 1);
	EXPECT_TRUE(sent[0].broadcast);
	EXPECT_PACKET_EQ(PacketTestUtils::FromBitStream(expected), PacketTestUtils::FromCapture(sent[0]));
}
