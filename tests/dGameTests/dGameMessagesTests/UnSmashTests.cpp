#include "GameMessages.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"

#include <gtest/gtest.h>

using namespace PacketTestUtils;

// UnSmash layout from the 1.10.64 client (GameMessage::UnSmash::Serialize @ 00dc0e80):
//   bit + i64 builderID, the i64 only if builderID != LWOOBJID_EMPTY
//   bit + f32 duration,  the f32 only if duration != 3.0f
// Expected bits below are hand computed (RakNet writes MSB first).
namespace {
	PacketBytes Payload(const GameMessages::UnSmash& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	GameMessages::UnSmash Make(LWOOBJID builderID, float duration) {
		GameMessages::UnSmash msg;
		msg.builderID = builderID;
		msg.duration = duration;
		return msg;
	}
}

TEST(UnSmashTests, DefaultsWriteOnlyTwoFlagBits) {
	// Before the fix, duration was written whenever builderID != 3.0f, i.e. always: 34 bits here.
	EXPECT_PACKET_EQ(FromHex("00", 2), Payload(Make(LWOOBJID_EMPTY, 3.0f)));
}

TEST(UnSmashTests, GoldenBytes) {
	EXPECT_PACKET_EQ(FromHex("84 03 83 02 82 01 81 00 80", 66), Payload(Make(0x0102030405060708LL, 3.0f)));
	EXPECT_PACKET_EQ(FromHex("40 00 30 0f c0", 34), Payload(Make(LWOOBJID_EMPTY, 1.5f)));
	EXPECT_PACKET_EQ(FromHex("84 03 83 02 82 01 81 00 c0 00 30 0f c0", 98), Payload(Make(0x0102030405060708LL, 1.5f)));
}

TEST(UnSmashTests, RoundTrip) {
	for (const LWOOBJID builderID : { LWOOBJID_EMPTY, LWOOBJID{ 0x0102030405060708LL } }) {
		for (const float duration : { 3.0f, 0.0f, 1.5f }) {
			const auto msg = Make(builderID, duration);
			RakNet::BitStream bitStream;
			msg.Serialize(bitStream);
			GameMessages::UnSmash read;
			read.builderID = 42;
			read.duration = 42.0f;
			ASSERT_TRUE(read.Deserialize(bitStream));
			EXPECT_EQ(read.builderID, builderID);
			EXPECT_EQ(read.duration, duration);
			EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
		}
	}
}
