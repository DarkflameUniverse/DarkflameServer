#include "GameDependencies.h"
#include "GameMessageTestUtils.h"

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
