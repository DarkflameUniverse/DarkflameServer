#include "ActivityMessages.h"
#include "GameDependencies.h"
#include "PacketTestUtils.h"
#include "Legacy/ActivityMessagesLegacy.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>

#include <gtest/gtest.h>

using namespace PacketTestUtils;

namespace {
	SystemAddress ClientAddress() {
		SystemAddress address;
		address.binaryAddress = 0x0100007f;
		address.port = 2003;
		return address;
	}

	const std::array<LWOOBJID, 3> g_Targets = { LWOOBJID_EMPTY, 0x1000000000000001LL, 0x0102030405060708LL };
	const std::array<SystemAddress, 2> g_Addresses = { ClientAddress(), UNASSIGNED_SYSTEM_ADDRESS };

	PacketBytes StructPacket(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.WritePacket(bitStream);
		return FromBitStream(bitStream);
	}

	// Sends the same message through the frozen legacy function and through the struct, to one client and as a
	// broadcast, and requires identical bytes and the same effective destination.
	void ExpectSameAsLegacy(const std::function<void(const SystemAddress&)>& legacySend, const GameMessages::NetGameMsg& msg) {
		for (const auto& address : g_Addresses) {
			SCOPED_TRACE(address == UNASSIGNED_SYSTEM_ADDRESS ? "broadcast" : "single client");
			const auto legacyPackets = Capture([&] { legacySend(address); });
			const auto newPackets = Capture([&] { msg.Send(address); });

			ASSERT_FALSE(legacyPackets.empty());
			ASSERT_EQ(newPackets.size(), 1);
			// The legacy broadcast path also did a second Send(UNASSIGNED_SYSTEM_ADDRESS, broadcast = false),
			// which RakPeer::Send rejects without sending anything. Every copy must still match byte for byte.
			for (const auto& legacyPacket : legacyPackets) {
				EXPECT_PACKET_EQ(FromCapture(legacyPacket), FromCapture(newPackets[0]));
			}
			EXPECT_EQ(legacyPackets[0].broadcast, newPackets[0].broadcast);
			EXPECT_EQ(legacyPackets[0].sysAddr, newPackets[0].sysAddr);
			EXPECT_PACKET_EQ(FromCapture(newPackets[0]), StructPacket(msg));
		}
	}

	// Serializes msg, reads it back into a fresh T and checks the fresh copy serializes to the same bytes.
	template<typename T>
	T RoundTrip(const T& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		T copy;
		EXPECT_TRUE(copy.Deserialize(bitStream));
		EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0);
		RakNet::BitStream again;
		copy.Serialize(again);
		EXPECT_PACKET_EQ(FromBitStream(bitStream), FromBitStream(again));
		return copy;
	}
}

class ActivityMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(ActivityMessagesTests, NoPayloadMessagesMatchLegacy) {
	for (const auto target : g_Targets) {
		GameMessages::ActivityEnter enter;
		enter.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendActivityEnter(target, a); }, enter);

		GameMessages::ActivityStart start;
		start.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendActivityStart(target, a); }, start);

		GameMessages::ActivityExit exit;
		exit.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendActivityExit(target, a); }, exit);
	}
}

TEST_F(ActivityMessagesTests, ActivityStopMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bExit : { false, true }) {
			for (const bool bUserCancel : { false, true }) {
				GameMessages::ActivityStop msg;
				msg.target = target;
				msg.bExit = bExit;
				msg.bUserCancel = bUserCancel;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendActivityStop(target, bExit, bUserCancel, a); }, msg);

				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bExit, bExit);
				EXPECT_EQ(copy.bUserCancel, bUserCancel);
			}
		}
	}
}

TEST_F(ActivityMessagesTests, ActivityPauseMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bPause : { false, true }) {
			GameMessages::ActivityPause msg;
			msg.target = target;
			msg.bPause = bPause;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendActivityPause(target, bPause, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).bPause, bPause);
		}
	}
}

TEST_F(ActivityMessagesTests, StartActivityTimeMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const float startTime : { 0.0f, 30.0f, -1.5f, 1e-30f, std::numeric_limits<float>::max() }) {
			GameMessages::StartActivityTime msg;
			msg.target = target;
			msg.startTime = startTime;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendStartActivityTime(target, startTime, a); }, msg);
			EXPECT_EQ(RoundTrip(msg).startTime, startTime);
		}
	}
}

TEST_F(ActivityMessagesTests, RequestActivityEnterMatchesLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bStart : { false, true }) {
			for (const LWOOBJID userID : g_Targets) {
				GameMessages::RequestActivityEnter msg;
				msg.target = target;
				msg.bStart = bStart;
				msg.userID = userID;
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendRequestActivityEnter(target, a, bStart, userID); }, msg);

				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bStart, bStart);
				EXPECT_EQ(copy.userID, userID);
			}
		}
	}
}

TEST_F(ActivityMessagesTests, ShowActivityCountdownMatchesLegacy) {
	const std::u16string longName(300, u'é');
	for (const auto target : g_Targets) {
		for (const bool bPlayAdditionalSound : { false, true }) {
			for (const bool bPlayCountdownSound : { false, true }) {
				for (const std::u16string& sndName : { std::u16string(), std::u16string(u"sfx/minigame/countdown"), longName }) {
					for (const int32_t state : { 0, 3, -1 }) {
						GameMessages::ShowActivityCountdown msg;
						msg.target = target;
						msg.bPlayAdditionalSound = bPlayAdditionalSound;
						msg.bPlayCountdownSound = bPlayCountdownSound;
						msg.sndName = sndName;
						msg.stateToPlaySoundOn = state;
						ExpectSameAsLegacy([&](const SystemAddress& a) {
							LegacyGameMessages::SendShowActivityCountdown(target, bPlayAdditionalSound, bPlayCountdownSound, sndName, state, a);
							}, msg);

						const auto copy = RoundTrip(msg);
						EXPECT_EQ(copy.bPlayAdditionalSound, bPlayAdditionalSound);
						EXPECT_EQ(copy.bPlayCountdownSound, bPlayCountdownSound);
						EXPECT_EQ(copy.sndName, sndName);
						EXPECT_EQ(copy.stateToPlaySoundOn, state);
					}
				}
			}
		}
	}
}

// Independent of the legacy code: pins the layout against hand computed bytes.
TEST_F(ActivityMessagesTests, GoldenBytes) {
	// 53 | 05 00 (CLIENT) | 0c 00 00 00 (GAME_MSG) | 00 | target (LE i64) | msgId (LE u16) | payload
	GameMessages::ActivityStop stop;
	stop.target = 0x0102030405060708LL;
	stop.bExit = true;
	stop.bUserCancel = false;
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 98 01 80", 146), StructPacket(stop));

	GameMessages::StartActivityTime time;
	time.target = 0x0102030405060708LL;
	time.startTime = 1.5f;
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 40 02 00 00 c0 3f"), StructPacket(time));
}

TEST_F(ActivityMessagesTests, RequestActivityExitReadsLikeLegacy) {
	for (const bool bUserCancel : { false, true }) {
		for (const LWOOBJID userID : g_Targets) {
			RakNet::BitStream wire;
			wire.Write(bUserCancel);
			wire.Write(userID);

			RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
			const auto legacy = LegacyGameMessages::ReadRequestActivityExit(legacyStream);

			RakNet::BitStream newStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
			GameMessages::RequestActivityExit msg;
			ASSERT_TRUE(msg.Deserialize(newStream));

			EXPECT_EQ(msg.bUserCancel, legacy.canceled);
			// The legacy handler only read userID (and only acted) when the exit was a user cancel.
			if (legacy.canceled) EXPECT_EQ(msg.userID, legacy.player_id);

			RakNet::BitStream reserialized;
			msg.Serialize(reserialized);
			EXPECT_PACKET_EQ(FromBitStream(wire), FromBitStream(reserialized));
		}
	}

	RakNet::BitStream truncated;
	truncated.Write(true);
	GameMessages::RequestActivityExit msg;
	EXPECT_FALSE(msg.Deserialize(truncated));
}
