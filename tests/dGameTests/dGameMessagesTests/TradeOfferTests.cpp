#include <gtest/gtest.h>

#include <map>

#include "GameDependencies.h"
#include "TradingManager.h"

namespace {
	const std::map<LWOOBJID, HeldTradeItem> g_Held = {
		{ 100, { 1727, 5 } },
		{ 200, { 3040, 1 } },
		{ 300, { 1727, 2, true, false } },
		{ 400, { 1727, 2, false, true } },
		{ 500, { 1727, 2, false, false, false, true } },
		{ 600, { 6662, 1, false, false, true, false } },
	};

	class TradeAcceptTest : public GameDependenciesTest {
	protected:
		void SetUp() override {
			SetUpDependencies();
		}

		void TearDown() override {
			TearDownDependencies();
		}
	};

	std::optional<HeldTradeItem> FindHeld(const LWOOBJID itemId) {
		const auto it = g_Held.find(itemId);
		if (it == g_Held.end()) return std::nullopt;
		return it->second;
	}
}

TEST(TradeOfferTest, AnOfferOfHeldItemsIsKept) {
	const std::vector<TradeItem> offered = { { 100, 1727, 3 }, { 200, 3040, 1 } };
	const auto resolved = Trade::ResolveOffer(offered, FindHeld);
	ASSERT_TRUE(resolved.has_value());
	EXPECT_EQ(*resolved, offered);
}

TEST(TradeOfferTest, AnEmptyOfferIsKept) {
	const auto resolved = Trade::ResolveOffer({}, FindHeld);
	ASSERT_TRUE(resolved.has_value());
	EXPECT_TRUE(resolved->empty());
}

TEST(TradeOfferTest, TheLotComesFromTheHeldItem) {
	const auto resolved = Trade::ResolveOffer({ { 100, 7777, 2 } }, FindHeld);
	ASSERT_TRUE(resolved.has_value());
	ASSERT_EQ(resolved->size(), 1);
	EXPECT_EQ(resolved->front().itemLot, 1727);
	EXPECT_EQ(resolved->front().itemCount, 2);
}

TEST(TradeOfferTest, TheCountStopsAtTheHeldCount) {
	const auto resolved = Trade::ResolveOffer({ { 100, 1727, 4000000000U } }, FindHeld);
	ASSERT_TRUE(resolved.has_value());
	ASSERT_EQ(resolved->size(), 1);
	EXPECT_EQ(resolved->front().itemCount, 5);
}

TEST(TradeOfferTest, AnItemOfferedTwiceIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 100, 1727, 3 }, { 100, 1727, 3 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, AnItemThePlayerDoesNotHoldIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 900, 1727, 1 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, ACountOfZeroIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 100, 1727, 0 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, ABoundItemIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 300, 1727, 1 } }, FindHeld).has_value());
	EXPECT_FALSE(Trade::ResolveOffer({ { 100, 1727, 1 }, { 300, 1727, 1 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, AnEquippedItemIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 400, 1727, 1 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, AnItemThatMayNotBeDeletedIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 500, 1727, 1 } }, FindHeld).has_value());
	EXPECT_FALSE(Trade::ResolveOffer({ { 100, 1727, 1 }, { 500, 1727, 1 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, AModelThatIsNotApprovedIsRefused) {
	EXPECT_FALSE(Trade::ResolveOffer({ { 600, 6662, 1 } }, FindHeld).has_value());
	EXPECT_FALSE(Trade::ResolveOffer({ { 100, 1727, 1 }, { 600, 6662, 1 } }, FindHeld).has_value());
}

TEST(TradeOfferTest, OnlyAnApprovedBrickBuiltModelIsApproved) {
	LwoNameValue config;
	// No moderation status
	EXPECT_FALSE(Trade::IsApproved(Trade::BrickBuiltModelLot, config));

	config.Insert<int32_t>(u"userModelMod", 0);
	EXPECT_FALSE(Trade::IsApproved(Trade::BrickBuiltModelLot, config));

	config.Insert<int32_t>(u"userModelMod", 2);
	EXPECT_FALSE(Trade::IsApproved(Trade::BrickBuiltModelLot, config));

	// Not a number
	config.Insert<bool>(u"userModelMod", true);
	EXPECT_FALSE(Trade::IsApproved(Trade::BrickBuiltModelLot, config));

	config.Insert<int32_t>(u"userModelMod", Trade::ApprovedModerationStatus);
	EXPECT_TRUE(Trade::IsApproved(Trade::BrickBuiltModelLot, config));
}

TEST(TradeOfferTest, AnItemThatIsNotAModelHasNoModerationStatus) {
	LwoNameValue config;
	EXPECT_TRUE(Trade::IsApproved(1727, config));

	config.Insert<int32_t>(u"userModelMod", 0);
	EXPECT_TRUE(Trade::IsApproved(1727, config));
}

TEST(TradeOfferTest, AnOfferFillsTheSlotsOfTheTradeWindowAtMost) {
	const auto findAny = [](const LWOOBJID) -> std::optional<HeldTradeItem> {
		return HeldTradeItem{ 1727, 1 };
	};

	std::vector<TradeItem> offered;
	for (LWOOBJID itemId = 1; itemId <= Trade::MaxOfferedItems; itemId++) offered.push_back({ itemId, 1727, 1 });
	EXPECT_TRUE(Trade::ResolveOffer(offered, findAny).has_value());

	offered.push_back({ Trade::MaxOfferedItems + 1, 1727, 1 });
	EXPECT_FALSE(Trade::ResolveOffer(offered, findAny).has_value());
}

TEST_F(TradeAcceptTest, OnlyTheFinalAcceptCounts) {
	Trade trade(1, 10, 20);
	EXPECT_FALSE(trade.GetAccepted(10));

	trade.SetAccepted(10, true);
	EXPECT_FALSE(trade.GetAccepted(10));

	trade.SetAccepted(10, false);
	EXPECT_TRUE(trade.GetAccepted(10));
	EXPECT_FALSE(trade.GetAccepted(20));
	// Not a participant
	EXPECT_FALSE(trade.GetAccepted(30));
}

TEST_F(TradeAcceptTest, AChangedOfferTakesBothAcceptsBack) {
	Trade trade(1, 10, 20);
	trade.SetAccepted(10, false);
	ASSERT_TRUE(trade.GetAccepted(10));

	// What the update handler does once the new offer is stored
	trade.SetItems(20, { { 100, 1727, 1 } });
	trade.ResetAccepted();
	EXPECT_FALSE(trade.GetAccepted(10));
	EXPECT_FALSE(trade.GetAccepted(20));

	// The other player's accept alone does not complete the trade, both have to accept again
	trade.SetAccepted(20, false);
	EXPECT_TRUE(trade.GetAccepted(20));
	EXPECT_FALSE(trade.GetAccepted(10));
}
