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

namespace {
	void MakePadTables() {
		CDClientDatabase::Connect(":memory:");
		CDClientDatabase::ExecuteDML("CREATE TABLE RocketLaunchpadControlComponent (id INTEGER, targetZone INTEGER, defaultZoneID INTEGER, targetScene TEXT, gmLevel INTEGER, useLaunchPrecondition INTEGER, launchPrecondition TEXT, useAltLandingPrecondition INTEGER, altLandingPrecondition TEXT, altLandingSpawnPointName TEXT);");
		// Row 3 is the Venture Explorer pad's (LOT 14214): no precondition in the table
		// No precondition is known: one that is asked for fails for a player without components
		CDClientDatabase::ExecuteDML("CREATE TABLE Preconditions (id INTEGER, type INTEGER, targetLOT TEXT, targetGroup TEXT, targetCount INTEGER, iconID INTEGER, localize INTEGER, validContexts INTEGER);");
		CDClientDatabase::ExecuteDML("INSERT INTO RocketLaunchpadControlComponent VALUES (3, 1100, 0, 'AGLanding', 0, 0, NULL, 0, NULL, NULL);");
		// Row 16: a Nimbus Station pad with the precondition in the table
		CDClientDatabase::ExecuteDML("INSERT INTO RocketLaunchpadControlComponent VALUES (16, 1300, 0, 'GFLanding', 0, 1, '0', 0, '', '');");
	}
}

// A pad whose level sets rocketLaunchUseDBDefaults to false is ruled by the level's keys, not its row:
// LWORocketLaunchComponentCommon::LoadConfigData (0x00c55150) reads them and the row is only loaded while the key is
// true (0x00c12ea0). The Venture Explorer's pads: the row has no precondition, the level has
// "237;8;127|128;6|55;107;282" with rocketUseLaunchPrecondition.
TEST_F(RocketLaunchTests, APadThatDoesNotUseTheTableIsRuledByItsLevel) {
	MakePadTables();
	Entity pad(PAD, info);
	pad.SetVar<bool>(u"rocketLaunchUseDBDefaults", false);
	pad.SetVar<int32_t>(u"targetZone", 1100);
	pad.SetVar<std::u16string>(u"targetScene", u"AGLanding");
	pad.SetVar<int32_t>(u"GMLevel", 0);
	pad.SetVar<bool>(u"rocketUseLaunchPrecondition", true);
	pad.SetVar<std::u16string>(u"rocketLaunchPreCondition", u"0"); // an empty precondition table: nobody passes "0"
	// AddComponent loads the row, then the level's keys
	auto* const component = pad.AddComponent<RocketLaunchpadControlComponent>(3);

	Entity player(PLAYER, info);
	// The row alone asks for nothing
	Entity rowPad(PAD + 2, info);
	EXPECT_TRUE(rowPad.AddComponent<RocketLaunchpadControlComponent>(3)->MayLaunch(player));
	EXPECT_EQ(component->GetTargetZone(), 1100);
	EXPECT_EQ(component->GetTargetScene(), "AGLanding");
	EXPECT_FALSE(component->MayLaunch(player));

	// The level's GM level counts as well (the test pads to the Gnarled Forest and Forbidden Valley properties: 9)
	Entity gmPad(PAD + 1, info);
	gmPad.SetVar<bool>(u"rocketLaunchUseDBDefaults", false);
	gmPad.SetVar<int32_t>(u"targetZone", 1100);
	gmPad.SetVar<int32_t>(u"GMLevel", 9);
	auto* const gmOnly = gmPad.AddComponent<RocketLaunchpadControlComponent>(3);
	EXPECT_FALSE(gmOnly->MayLaunch(player));
	player.SetGMLevel(eGameMasterLevel::OPERATOR);
	EXPECT_TRUE(gmOnly->MayLaunch(player));
}

// The level's precondition is only used with the level's rocketUseLaunchPrecondition (Nimbus Station's pad to
// Forbidden Valley's property has "115" without it)
TEST_F(RocketLaunchTests, ALevelPreconditionThatIsNotInUseIsNotChecked) {
	MakePadTables();
	Entity pad(PAD, info);
	pad.SetVar<bool>(u"rocketLaunchUseDBDefaults", false);
	pad.SetVar<int32_t>(u"targetZone", 1450);
	pad.SetVar<bool>(u"rocketUseLaunchPrecondition", false);
	pad.SetVar<std::u16string>(u"rocketLaunchPreCondition", u"0");
	auto* const component = pad.AddComponent<RocketLaunchpadControlComponent>(3);
	Entity player(PLAYER, info);
	EXPECT_TRUE(component->MayLaunch(player));
	EXPECT_EQ(component->GetTargetZone(), 1450);
}

// With rocketLaunchUseDBDefaults true (the client's default, and 12 pads in the maps say so) the row rules and the
// level's keys are not looked at
TEST_F(RocketLaunchTests, APadThatUsesTheTableIgnoresItsLevelKeys) {
	MakePadTables();
	for (const bool withKey : { true, false }) {
		Entity pad(PAD, info);
		if (withKey) pad.SetVar<bool>(u"rocketLaunchUseDBDefaults", true);
		pad.SetVar<int32_t>(u"targetZone", 1400);
		pad.SetVar<std::u16string>(u"targetScene", u"FVGate");
		pad.SetVar<int32_t>(u"GMLevel", 9);
		pad.SetVar<bool>(u"rocketUseLaunchPrecondition", false);
		auto* const component = pad.AddComponent<RocketLaunchpadControlComponent>(16);
		EXPECT_EQ(component->GetTargetZone(), 1300);
		EXPECT_EQ(component->GetTargetScene(), "GFLanding");
		Entity player(PLAYER, info);
		EXPECT_FALSE(component->MayLaunch(player)); // the row's precondition, not the level's "none"
	}
}
