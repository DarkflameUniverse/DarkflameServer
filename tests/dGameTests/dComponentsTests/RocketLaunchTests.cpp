#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "CDClientDatabase.h"
#include "Entity.h"
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

// A pad's row can ask for a GM level (the AG test pad: 9); LWORocketLaunchComponentCommon::msgFireEventClientSide
// (0x00cb78b0) does not launch a player under it
TEST_F(RocketLaunchTests, APadWithAGmLevelLaunchesOnlyThoseAtIt) {
	CDClientDatabase::Connect(":memory:");
	CDClientDatabase::ExecuteDML("CREATE TABLE RocketLaunchpadControlComponent (id INTEGER, targetZone INTEGER, defaultZoneID INTEGER, targetScene TEXT, gmLevel INTEGER, useLaunchPrecondition INTEGER, launchPrecondition TEXT, useAltLandingPrecondition INTEGER, altLandingPrecondition TEXT, altLandingSpawnPointName TEXT);");
	CDClientDatabase::ExecuteDML("INSERT INTO RocketLaunchpadControlComponent VALUES (1, 1100, 0, 'AGLaunch', 9, 0, '', 0, '', '');");
	CDClientDatabase::ExecuteDML("INSERT INTO RocketLaunchpadControlComponent VALUES (2, 1200, 0, 'NS_NT', 0, 0, '47', 0, '', '');");

	Entity gmPad(PAD, info);
	auto* const gmOnly = gmPad.AddComponent<RocketLaunchpadControlComponent>(1);
	gmOnly->LoadDataFromTemplate();
	Entity openPad(PAD + 1, info);
	auto* const open = openPad.AddComponent<RocketLaunchpadControlComponent>(2);
	open->LoadDataFromTemplate();

	Entity player(PLAYER, info);
	EXPECT_FALSE(gmOnly->MayLaunch(player));
	EXPECT_TRUE(open->MayLaunch(player)); // its precondition is not in use
	player.SetGMLevel(eGameMasterLevel::OPERATOR);
	EXPECT_TRUE(gmOnly->MayLaunch(player));
}
