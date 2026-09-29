#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "ObjectMessages.h"
#include "RocketLaunchpadControlComponent.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// The launchpad's RocketEquipped event, compared with live. Each expected packet is a live one with the launchpad,
// rocket and player IDs replaced.
class RocketLaunchTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	static constexpr LWOOBJID PAD = 0x0000400000112233LL;
	static constexpr LWOOBJID ROCKET = 0x1000000000000002LL;
	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;
};

TEST_F(RocketLaunchTests, ZoneLaunchHasNoParameters) {
	const auto msg = RocketLaunchpadControlComponent::MakeRocketEquipped(PAD, ROCKET, PLAYER, LWOCLONEID_INVALID, -1);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 33 22 11 00 00 40 00 00 bd 04 0e 00 00 00 52 00 6f 00 63 00 6b 00 65 00 74 00 45 00 71 00 "
		"75 00 69 00 70 00 70 00 65 00 64 00 02 00 00 00 00 00 00 10 00 40 00 00 00 00 00 04 00", 530), StructPacket(msg));
}

TEST_F(RocketLaunchTests, PropertyLaunchCarriesTheClone) {
	const auto msg = RocketLaunchpadControlComponent::MakeRocketEquipped(PAD, ROCKET, PLAYER, 338862, -1);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 33 22 11 00 00 40 00 00 bd 04 0e 00 00 00 52 00 6f 00 63 00 6b 00 65 00 74 00 45 00 71 00 "
		"75 00 69 00 70 00 70 00 65 00 64 00 02 00 00 00 00 00 00 10 d7 15 82 80 00 00 00 00 00 40 00 00 00 00 00 04 00", 594), StructPacket(msg));
}

TEST_F(RocketLaunchTests, LupLaunchCarriesTheWorldIndex) {
	const auto msg = RocketLaunchpadControlComponent::MakeRocketEquipped(PAD, ROCKET, PLAYER, 0, 0);
	EXPECT_PACKET_EQ(FromHex(
		"53 05 00 0c 00 00 00 00 33 22 11 00 00 40 00 00 bd 04 0e 00 00 00 52 00 6f 00 63 00 6b 00 65 00 74 00 45 00 71 00 "
		"75 00 69 00 70 00 70 00 65 00 64 00 02 00 00 00 00 00 00 10 40 00 00 00 00 40 00 00 00 00 00 04 00", 562), StructPacket(msg));
}

// Sent to every client (Send with UNASSIGNED_SYSTEM_ADDRESS broadcasts), as Launch does.
TEST_F(RocketLaunchTests, RocketEquippedIsBroadcast) {
	const auto sent = Capture([&] {
		RocketLaunchpadControlComponent::MakeRocketEquipped(PAD, ROCKET, PLAYER, LWOCLONEID_INVALID, -1).Send(UNASSIGNED_SYSTEM_ADDRESS);
		});
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_TRUE(sent[0].broadcast);
}
