#include <gtest/gtest.h>

#include "EconomyLedger.h"

using namespace EconomyLedger;

TEST(EconomyLedgerTests, SumsFlowsPerDayAndSource) {
	Accumulator ledger;
	EXPECT_TRUE(ledger.Empty());
	ledger.AddCoins(10, 1, 2, 100);
	ledger.AddCoins(10, 1, 2, -30);
	ledger.AddCoins(10, 1, 2, 50);
	ledger.AddCoins(11, 1, 2, 5);
	ledger.AddCoins(10, 1, 9, 0); // no change, no row
	EXPECT_FALSE(ledger.Empty());

	const auto batch = ledger.Take();
	EXPECT_TRUE(ledger.Empty());
	ASSERT_EQ(batch.currency.size(), 2);
	EXPECT_EQ(batch.currency[0].day, 10);
	EXPECT_EQ(batch.currency[0].gained, 150);
	EXPECT_EQ(batch.currency[0].spent, 30);
	EXPECT_EQ(batch.currency[1].day, 11);
	EXPECT_EQ(batch.currency[1].gained, 5);
}

TEST(EconomyLedgerTests, KeepsStaffItemsApart) {
	Accumulator ledger;
	ledger.AddItems(10, 1727, 1, false, 3);
	ledger.AddItems(10, 1727, 1, true, 3);
	ledger.AddItems(10, 1727, 1, false, -1);
	ledger.AddUScore(10, 7, 2, 500);
	ledger.AddUScore(10, 7, 2, -200);

	const auto batch = ledger.Take();
	ASSERT_EQ(batch.items.size(), 2);
	EXPECT_FALSE(batch.items[0].gm);
	EXPECT_EQ(batch.items[0].created, 3);
	EXPECT_EQ(batch.items[0].destroyed, 1);
	EXPECT_TRUE(batch.items[1].gm);
	ASSERT_EQ(batch.uscore.size(), 1);
	EXPECT_EQ(batch.uscore[0].gained, 500);
	EXPECT_EQ(batch.uscore[0].lost, 200);
}

TEST(EconomyLedgerTests, BucketsMapEventsIntoCells) {
	Accumulator ledger;
	const auto cell = IEconomyLedger::MAP_CELL_SIZE;
	ledger.AddMapEvent(10, 1100, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, 1.0f, 1.0f, 1);
	ledger.AddMapEvent(10, 1100, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, cell - 0.5f, 3.0f, 1);  // same cell
	ledger.AddMapEvent(10, 1100, 0, IEconomyLedger::eMapEvent::ENEMY_KILLS, 4712, -1.0f, 1.0f, 1);        // negative side
	ledger.AddMapEvent(10, 1100, 0, IEconomyLedger::eMapEvent::COIN_DROPS, 0, 1.0f, 1.0f, 250);

	const auto batch = ledger.Take();
	ASSERT_EQ(batch.mapEvents.size(), 3);
	int64_t kills = 0;
	for (const auto& event : batch.mapEvents) {
		if (event.kind == IEconomyLedger::eMapEvent::ENEMY_KILLS && event.cellX == 0 && event.cellZ == 0) EXPECT_EQ(event.events, 2);
		if (event.kind == IEconomyLedger::eMapEvent::ENEMY_KILLS && event.cellX == -1) EXPECT_EQ(event.events, 1);
		if (event.kind == IEconomyLedger::eMapEvent::ENEMY_KILLS) kills += event.events;
		if (event.kind == IEconomyLedger::eMapEvent::COIN_DROPS) {
			EXPECT_EQ(event.events, 1);
			EXPECT_EQ(event.quantity, 250);
		}
	}
	EXPECT_EQ(kills, 3);
}

TEST(EconomyLedgerTests, SumsPlayerStatsPerDayZoneAndStaff) {
	Accumulator ledger;
	ledger.AddStat(10, 1100, 0, SmashablesSmashed, false, 1);
	ledger.AddStat(10, 1100, 0, SmashablesSmashed, false, 2);
	ledger.AddStat(10, 1100, 0, SmashablesSmashed, true, 1);   // staff apart
	ledger.AddStat(10, 1200, 0, SmashablesSmashed, false, 1);  // another zone
	ledger.AddStat(10, 1100, 0, MetersTraveled, false, 0);     // nothing to add, no row
	EXPECT_FALSE(ledger.Empty());

	const auto batch = ledger.Take();
	EXPECT_TRUE(ledger.Empty());
	ASSERT_EQ(batch.stats.size(), 3);
	EXPECT_EQ(batch.stats[0].zone, 1100);
	EXPECT_EQ(batch.stats[0].stat, static_cast<uint32_t>(SmashablesSmashed));
	EXPECT_FALSE(batch.stats[0].gm);
	EXPECT_EQ(batch.stats[0].amount, 3);
	EXPECT_TRUE(batch.stats[1].gm);
	EXPECT_EQ(batch.stats[2].zone, 1200);
}

TEST(EconomyLedgerTests, KeepsEachPropertyApart) {
	// Two properties on the same zone, same place: their events and statistics stay apart, and apart from clone 0
	Accumulator ledger;
	ledger.AddMapEvent(10, 1150, 0, IEconomyLedger::eMapEvent::POWERUP_DROPS, 177, 1.0f, 1.0f, 1);
	ledger.AddMapEvent(10, 1150, 7, IEconomyLedger::eMapEvent::POWERUP_DROPS, 177, 1.0f, 1.0f, 1);
	ledger.AddMapEvent(10, 1150, 7, IEconomyLedger::eMapEvent::POWERUP_DROPS, 177, 1.5f, 1.5f, 1);
	ledger.AddMapEvent(10, 1150, 9, IEconomyLedger::eMapEvent::POWERUP_DROPS, 177, 1.0f, 1.0f, 1);
	ledger.AddStat(10, 1150, 7, SmashablesSmashed, false, 2);
	ledger.AddStat(10, 1150, 9, SmashablesSmashed, false, 1);

	const auto batch = ledger.Take();
	ASSERT_EQ(batch.mapEvents.size(), 3);
	EXPECT_EQ(batch.mapEvents[0].clone, 0);
	EXPECT_EQ(batch.mapEvents[1].clone, 7);
	EXPECT_EQ(batch.mapEvents[1].events, 2);
	EXPECT_EQ(batch.mapEvents[2].clone, 9);
	ASSERT_EQ(batch.stats.size(), 2);
	EXPECT_EQ(batch.stats[0].clone, 7);
	EXPECT_EQ(batch.stats[0].amount, 2);
	EXPECT_EQ(batch.stats[1].clone, 9);
}

TEST(EconomyLedgerTests, TransfersAreKeptInOrder) {
	Accumulator ledger;
	ledger.AddTransfer({ .time = 1, .method = IEconomyLedger::eTransferMethod::TRADE, .itemId = 5 });
	ledger.AddTransfer({ .time = 2, .method = IEconomyLedger::eTransferMethod::MAIL_SENT, .itemId = 6 });
	const auto batch = ledger.Take();
	ASSERT_EQ(batch.transfers.size(), 2);
	EXPECT_EQ(batch.transfers[0].itemId, 5);
	EXPECT_EQ(batch.transfers[1].method, IEconomyLedger::eTransferMethod::MAIL_SENT);
	EXPECT_TRUE(ledger.Take().transfers.empty());
}
