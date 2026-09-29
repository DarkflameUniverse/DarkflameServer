#include "GameDependencies.h"
#include "GameMessageTestUtils.h"

#include "MovementMessages.h"
#include "ZoneMessages.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// What the world sends while a player loads into a zone, compared with the live captures.
class LoadSequenceTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;
	static constexpr LWOOBJID ZONE_CONTROL = 0x3FFF'FFFFFFFELL;
};

// Live: PlayerReady to the player, then to the zone control object, both to the loading client only.
// Bytes: the live pair with the player ID replaced.
TEST_F(LoadSequenceTests, PlayerReadyGoesToThePlayerThenTheZoneControl) {
	const auto sent = Capture([&] { GameMessages::SendPlayerReady(PLAYER, ZONE_CONTROL, ClientAddress()); });
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 fd 01"), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 fe ff ff ff ff 3f 00 00 fd 01"), FromCapture(sent[1]));
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, ClientAddress());
		EXPECT_FALSE(packet.broadcast);
	}
}

TEST_F(LoadSequenceTests, PlayerReadyWithoutZoneControlGoesToThePlayerOnly) {
	const auto sent = Capture([&] { GameMessages::SendPlayerReady(PLAYER, LWOOBJID_EMPTY, ClientAddress()); });
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 fd 01"), FromCapture(sent[0]));
}

// Live: ServerDoneLoadingAllObjects, then right after it the respawn checkpoint with a rotation. Bytes: a live pair
// from Avant Gardens Survival (a first load, so the spawn point) with the player ID replaced.
TEST_F(LoadSequenceTests, RespawnCheckpointFollowsDoneLoading) {
	const NiPoint3 spawn(35.218f, 365.7804f, -201.3283f);
	const NiQuaternion facing(0.7015f, 0.0f, -0.7126f, 0.0f);
	const auto sent = Capture([&] { GameMessages::SendDoneLoading(PLAYER, NiPoint3Constant::ZERO, spawn, facing, ClientAddress()); });
	ASSERT_EQ(sent.size(), 2u);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 6a 06"), FromCapture(sent[0]));
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 10 05 3b df 0c 42 e4 e3 b6 43 0b 54 49 c3 c0 ca 99 9f 80 00 00 00 "
		"7a 36 1b 5f 80 00 00 00 00", 369), FromCapture(sent[1]));
	for (const auto& packet : sent) {
		EXPECT_EQ(packet.sysAddr, ClientAddress());
		EXPECT_FALSE(packet.broadcast);
	}
}

// A saved checkpoint wins over where the player loaded in; DLU keeps no rotation for it.
TEST_F(LoadSequenceTests, SavedCheckpointIsSentUnrotated) {
	const NiPoint3 saved(100.0f, 200.0f, 300.0f);
	const auto sent = Capture([&] { GameMessages::SendDoneLoading(PLAYER, saved, NiPoint3(1.0f, 2.0f, 3.0f), NiQuaternion(0.7015f, 0.0f, -0.7126f, 0.0f), ClientAddress()); });
	const auto checkpoints = SentGameMessages<GameMessages::PlayerReachedRespawnCheckpoint>(sent);
	ASSERT_EQ(checkpoints.size(), 1u);
	EXPECT_EQ(checkpoints[0].pos, saved);
	EXPECT_EQ(checkpoints[0].rot, QuatUtils::IDENTITY);
}
