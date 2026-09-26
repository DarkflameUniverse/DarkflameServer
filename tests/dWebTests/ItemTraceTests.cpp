#include <gtest/gtest.h>

#include <algorithm>

#include "ItemTrace.h"

using namespace ItemTrace;
using enum IEconomyLedger::eTransferMethod;

namespace {
	constexpr LWOOBJID ALICE = 1001;
	constexpr LWOOBJID BOB = 1002;
	constexpr LWOOBJID CAROL = 1003;

	// economy_transfers in memory: rows touching any of the ids, like GetTransfersForItems
	struct Ledger {
		std::vector<Hop> rows;
		size_t queries = 0;

		void Add(int64_t time, eTransferMethod method, LWOOBJID from, LWOOBJID to, LWOOBJID fromChar, LWOOBJID toChar, std::optional<bool> merged = false) {
			rows.push_back({ static_cast<int64_t>(rows.size() + 1), time, method, from, to, fromChar, toChar, merged });
		}

		Fetch Fetcher() {
			return [this](const std::vector<LWOOBJID>& ids) {
				queries++;
				std::vector<Hop> found;
				for (const auto& row : rows) {
					if (std::ranges::find(ids, row.itemId) != ids.end() || std::ranges::find(ids, row.newItemId) != ids.end()) found.push_back(row);
				}
				return found;
			};
		}
	};

	const TracedHop* HopFrom(const Chain& chain, LWOOBJID from, LWOOBJID to) {
		for (const auto& hop : chain.hops) {
			if (hop.hop.itemId == from && hop.hop.newItemId == to) return &hop;
		}
		return nullptr;
	}

	bool Contains(const std::vector<LWOOBJID>& ids, LWOOBJID id) { return std::ranges::find(ids, id) != ids.end(); }
}

TEST(ItemTraceTests, FollowsTheWholeChainFromTheMiddle) {
	Ledger ledger;
	ledger.Add(10, TRADE, 1, 2, ALICE, BOB);
	ledger.Add(20, MAIL_SENT, 2, 3, BOB, CAROL);
	ledger.Add(30, MAIL_CLAIMED, 3, 4, BOB, CAROL);
	ledger.Add(40, INVENTORY_MOVE, 4, 5, CAROL, CAROL);
	ledger.Add(50, TRADE, 5, 6, CAROL, ALICE);
	ledger.Add(60, TRADE, 99, 98, ALICE, BOB); // unrelated

	const auto chain = Build(3, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 5u);
	EXPECT_EQ(chain.first, 1);
	EXPECT_EQ(chain.latest, std::vector<LWOOBJID>{ 6 });
	EXPECT_FALSE(chain.truncated);
	for (size_t i = 0; i < chain.hops.size(); i++) {
		EXPECT_EQ(chain.hops[i].role, eRole::LINE);
		EXPECT_FALSE(chain.hops[i].merge);
		EXPECT_FALSE(chain.hops[i].gap.has_value()) << i;
		if (i > 0) EXPECT_LT(chain.hops[i - 1].hop.time, chain.hops[i].hop.time); // oldest first
	}
	EXPECT_EQ(chain.ids.size(), 6u);
}

TEST(ItemTraceTests, SplitStacksAreBranches) {
	Ledger ledger;
	ledger.Add(10, TRADE, 1, 2, ALICE, BOB);   // part of stack 1 to Bob
	ledger.Add(20, TRADE, 1, 3, ALICE, CAROL); // another part to Carol
	ledger.Add(30, TRADE, 3, 4, CAROL, BOB);

	const auto chain = Build(2, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 3u);
	EXPECT_EQ(HopFrom(chain, 1, 2)->role, eRole::LINE);
	EXPECT_EQ(HopFrom(chain, 1, 3)->role, eRole::BRANCH);
	EXPECT_EQ(HopFrom(chain, 3, 4)->role, eRole::BRANCH);
	EXPECT_EQ(chain.first, 1);
	EXPECT_TRUE(Contains(chain.latest, 2));
	EXPECT_TRUE(Contains(chain.latest, 4));
	EXPECT_FALSE(Contains(chain.latest, 1)); // left, though part may still be there (the locations say)

	// Searching the stack itself: both parts are on its line
	const auto fromStack = Build(1, ledger.Fetcher());
	EXPECT_EQ(HopFrom(fromStack, 1, 3)->role, eRole::LINE);
	EXPECT_EQ(HopFrom(fromStack, 3, 4)->role, eRole::LINE);
}

TEST(ItemTraceTests, MergeIntoAnExistingStackLeavesItsHistoryOut) {
	Ledger ledger;
	ledger.Add(10, TRADE, 50, 51, CAROL, BOB);             // stack 51's own history
	ledger.Add(20, MAIL_SENT, 1, 2, ALICE, BOB);
	ledger.Add(30, MAIL_CLAIMED, 2, 51, ALICE, BOB, true); // claimed into Bob's stack 51
	ledger.Add(40, TRADE, 51, 60, BOB, CAROL);             // the stack moves on, with the item in it

	auto chain = Build(1, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 3u);
	EXPECT_EQ(HopFrom(chain, 50, 51), nullptr);
	EXPECT_TRUE(HopFrom(chain, 2, 51)->merge);
	EXPECT_FALSE(HopFrom(chain, 2, 51)->mergeProven);
	EXPECT_EQ(HopFrom(chain, 51, 60)->role, eRole::LINE);
	EXPECT_FALSE(HopFrom(chain, 51, 60)->gap.has_value());
	EXPECT_EQ(chain.latest, std::vector<LWOOBJID>{ 60 });

	// Asked for: the other stack's history comes too, as a branch
	chain = Build(1, ledger.Fetcher(), 200, true);
	ASSERT_NE(HopFrom(chain, 50, 51), nullptr);
	EXPECT_EQ(HopFrom(chain, 50, 51)->role, eRole::BRANCH);
}

TEST(ItemTraceTests, MergeIsWorkedOutForOlderRows) {
	Ledger ledger;
	ledger.Add(10, TRADE, 50, 51, CAROL, BOB, std::nullopt);
	ledger.Add(20, TRADE, 1, 51, ALICE, BOB, std::nullopt); // 51 already had a row, so this joined it
	ledger.Add(30, TRADE, 7, 8, ALICE, BOB, std::nullopt);  // 8 is new

	auto chain = Build(1, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 1u);
	EXPECT_TRUE(chain.hops[0].merge);
	EXPECT_TRUE(chain.hops[0].mergeProven);

	chain = Build(7, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 1u);
	EXPECT_FALSE(chain.hops[0].merge);
}

TEST(ItemTraceTests, OtherItemsJoiningAreListedNotFollowed) {
	Ledger ledger;
	ledger.Add(10, TRADE, 1, 2, ALICE, BOB);
	ledger.Add(15, TRADE, 70, 71, CAROL, CAROL);       // history of the joining item
	ledger.Add(20, TRADE, 71, 2, CAROL, BOB, true);    // joins Bob's stack 2

	const auto chain = Build(1, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 2u);
	EXPECT_EQ(HopFrom(chain, 71, 2)->role, eRole::MERGE_IN);
	EXPECT_EQ(HopFrom(chain, 70, 71), nullptr);
	EXPECT_EQ(chain.latest, std::vector<LWOOBJID>{ 2 });
	EXPECT_FALSE(Contains(chain.ids, 71));

	// Searching the stack: its creation is the line, the joining item a merge in
	const auto fromStack = Build(2, ledger.Fetcher());
	EXPECT_EQ(HopFrom(fromStack, 1, 2)->role, eRole::LINE);
	EXPECT_EQ(HopFrom(fromStack, 71, 2)->role, eRole::MERGE_IN);
	EXPECT_EQ(fromStack.first, 1);
}

TEST(ItemTraceTests, CyclesEnd) {
	Ledger ledger;
	ledger.Add(10, TRADE, 1, 2, ALICE, BOB);
	ledger.Add(20, TRADE, 2, 3, BOB, ALICE);
	ledger.Add(30, TRADE, 3, 1, ALICE, BOB); // an id coming back (restored with its original id)
	ledger.Add(40, TRADE, 1, 1, BOB, BOB);   // old rows that kept the id

	const auto chain = Build(2, ledger.Fetcher());
	EXPECT_EQ(chain.hops.size(), 4u);
	EXPECT_FALSE(chain.truncated);
	EXPECT_LT(chain.queries, 10u);
}

TEST(ItemTraceTests, StopsAtTheHopLimit) {
	Ledger ledger;
	for (LWOOBJID id = 1; id <= 300; id++) ledger.Add(id, TRADE, id, id + 1, id % 2 ? ALICE : BOB, id % 2 ? BOB : ALICE);

	const auto chain = Build(150, ledger.Fetcher(), 200);
	EXPECT_TRUE(chain.truncated);
	EXPECT_EQ(chain.hops.size(), 200u);

	const auto all = Build(150, ledger.Fetcher(), 1000);
	EXPECT_FALSE(all.truncated);
	EXPECT_EQ(all.hops.size(), 300u);
	EXPECT_EQ(all.first, 1);
	EXPECT_EQ(all.latest, std::vector<LWOOBJID>{ 301 });
}

TEST(ItemTraceTests, MarksGaps) {
	Ledger ledger;
	ledger.Add(10, MAIL_CLAIMED, 1, 2, ALICE, BOB);  // the sending was not recorded
	ledger.Add(20, TRADE, 2, 3, CAROL, ALICE);       // Bob had it, not Carol
	ledger.Add(30, TRADE, 3, 0, ALICE, BOB);         // no new id

	const auto chain = Build(2, ledger.Fetcher());
	ASSERT_EQ(chain.hops.size(), 3u);
	EXPECT_EQ(HopFrom(chain, 1, 2)->gap, eGap::MAIL_NOT_SENT);
	EXPECT_EQ(HopFrom(chain, 2, 3)->gap, eGap::OWNER);
	EXPECT_EQ(HopFrom(chain, 3, 0)->gap, eGap::NO_NEW_ID);
}

TEST(ItemTraceTests, NoRows) {
	Ledger ledger;
	const auto chain = Build(5, ledger.Fetcher());
	EXPECT_TRUE(chain.hops.empty());
	EXPECT_EQ(chain.first, 5);
	EXPECT_EQ(chain.latest, std::vector<LWOOBJID>{ 5 });
	EXPECT_EQ(Build(0, ledger.Fetcher()).hops.size(), 0u);
}

TEST(ItemTraceTests, ReadsDatabaseRows) {
	const auto hop = FromRow({ {"id", 7}, {"time", 100}, {"method", 3}, {"item_id", "1152921510000700001"}, {"new_item_id", "1152921510000700002"},
		{"from_character", "11"}, {"to_character", "12"}, {"merged", nullptr} });
	EXPECT_EQ(hop.row, 7);
	EXPECT_EQ(hop.method, MAIL_CLAIMED);
	EXPECT_EQ(hop.itemId, 1152921510000700001LL);
	EXPECT_EQ(hop.newItemId, 1152921510000700002LL);
	EXPECT_EQ(hop.toCharacter, 12);
	EXPECT_FALSE(hop.merged.has_value());
	EXPECT_TRUE(FromRow({ {"item_id", "1"}, {"new_item_id", "2"}, {"from_character", "0"}, {"to_character", "0"}, {"merged", true} }).merged.value());
}
