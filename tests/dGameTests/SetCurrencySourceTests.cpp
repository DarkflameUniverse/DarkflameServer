#include "GameDependencies.h"
#include "dGameMessagesTests/GameMessageTestUtils.h"

#include "Character.h"
#include "Entity.h"
#include "PlayerMessages.h"
#include "eLootSourceType.h"

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

// SetCurrency source fields per source type, as live sent them. Each expected packet is a live SetCurrency with the
// player ID replaced by 0x1000000000000001 and any other object or trade ID by 0x0102030405060708.
class SetCurrencySourceTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }

	static constexpr LWOOBJID PLAYER = 0x1000000000000001LL;
	static constexpr LWOOBJID OTHER = 0x0102030405060708LL;
};

TEST_F(SetCurrencySourceTests, PickupCarriesThePickupPosition) {
	const auto msg = Character::MakeSetCurrency(PLAYER, 253493, eLootSourceType::PICKUP, CoinSource::Position(NiPoint3(-142.26820373535156f, 69.87815856933594f, 391.2568359375f)));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 35 de 03 00 00 00 00 00 54 a2 07 61 cf 60 c5 a1 70 50 61 a1 88 58 00 00 00", 341), StructPacket(msg));
}

TEST_F(SetCurrencySourceTests, MissionAndAchievementNameThePlayer) {
	auto playerInfo = info;
	playerInfo.lot = 1;
	Entity player(PLAYER, playerInfo);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 02 25 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 40 00 00 20 20 00 00 00 00 00 02 08 10 00 00 00", 437),
		StructPacket(Character::MakeSetCurrency(PLAYER, 206082, eLootSourceType::MISSION, CoinSource::Object(player))));
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 0a ef 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 40 40 00 00 20 20 00 00 00 00 00 02 08 28 00 00 00", 437),
		StructPacket(Character::MakeSetCurrency(PLAYER, 257802, eLootSourceType::ACHIEVEMENT, CoinSource::Object(player))));
}

TEST_F(SetCurrencySourceTests, VendorNamesTheVendor) {
	auto vendorInfo = info;
	vendorInfo.lot = 8212;
	Entity vendor(OTHER, vendorInfo);
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 18 39 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 45 08 00 00 21 00 e0 c0 a0 80 60 40 28 48 00 00 00", 437),
		StructPacket(Character::MakeSetCurrency(PLAYER, 14616, eLootSourceType::VENDOR, CoinSource::Object(vendor))));
}

TEST_F(SetCurrencySourceTests, TradeCarriesAnEightByteTradeId) {
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 34 66 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 80 70 60 50 40 30 20 18 30 00 00 00", 405),
		StructPacket(Character::MakeSetCurrency(PLAYER, 222772, eLootSourceType::TRADE, CoinSource::Trade(OTHER))));
}

TEST_F(SetCurrencySourceTests, DeathNamesNothing) {
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 01 00 00 00 00 00 00 10 85 00 61 59 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 40 00 00 00", 341),
		StructPacket(Character::MakeSetCurrency(PLAYER, 22881, eLootSourceType::DELETION, {})));
}
