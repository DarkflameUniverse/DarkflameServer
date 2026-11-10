#include <gtest/gtest.h>

#include <map>

#include "TradingManager.h"

namespace {
	const std::map<LWOOBJID, HeldTradeItem> g_Held = {
		{ 100, { 1727, 5 } },
		{ 200, { 3040, 1 } },
		{ 300, { 1727, 2, true, false } },
		{ 400, { 1727, 2, false, true } },
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
