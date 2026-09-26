#include "VendorMessages.h"
#include "TradeMessages.h"
#include "GameDependencies.h"
#include "GameMessageTestUtils.h"
#include "Legacy/VendorMessagesLegacy.h"

#include "eVendorTransactionResult.h"

#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace GameMessageTestUtils;

namespace {
	const std::vector<int32_t> g_Ints = { 0, 1, -1, 5, 1727, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
	const std::vector<std::u16string> g_Names = { u"", u"A", u"Mythran", u"é中文", std::u16string(300, u'x') };

	PacketBytes Payload(const GameMessages::NetGameMsg& msg) {
		RakNet::BitStream bitStream;
		msg.Serialize(bitStream);
		return FromBitStream(bitStream);
	}

	// Serializes msg, then reads it with the legacy read sequence; both must consume exactly the same bits.
	template<typename Result>
	Result ReadWithLegacy(const GameMessages::NetGameMsg& msg, const std::function<Result(RakNet::BitStream&)>& read) {
		RakNet::BitStream wire;
		msg.Serialize(wire);
		RakNet::BitStream legacyStream(wire.GetData(), wire.GetNumberOfBytesUsed(), false);
		auto result = read(legacyStream);
		EXPECT_EQ(legacyStream.GetReadOffset(), wire.GetNumberOfBitsUsed());
		return result;
	}

	// The entry DLU writes for a traded item.
	GameMessages::TradeItemEntry EntryFor(const TradeItem& item) {
		GameMessages::TradeItemEntry entry;
		entry.key = item.itemId;
		entry.itemID = item.itemId;
		entry.templateID = item.itemLot;
		entry.count = item.itemCount;
		return entry;
	}
}

class VendorMessagesTests : public GameDependenciesTest {
protected:
	void SetUp() override { SetUpDependencies(); }
	void TearDown() override { TearDownDependencies(); }
};

TEST_F(VendorMessagesTests, VendorOpenWindowMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		GameMessages::VendorOpenWindow msg;
		msg.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVendorOpenWindow(&entity, a); }, msg, SendMode::SendToClient);
		RoundTrip(msg);
	}
}

TEST_F(VendorMessagesTests, VendorStatusUpdateMatchesLegacy) {
	std::vector<SoldItem> many;
	for (int32_t i = 0; i < 300; i++) many.emplace_back(1000 + i, i);
	const std::vector<std::vector<SoldItem>> stocks = { {}, { SoldItem(1727, 0) }, { SoldItem(-1, -1), SoldItem(0, std::numeric_limits<int32_t>::max()), SoldItem(16253, 2) }, many };
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto& stock : stocks) {
			for (const bool bUpdateOnly : { false, true }) {
				GameMessages::VendorStatusUpdate msg;
				msg.target = target;
				msg.bUpdateOnly = bUpdateOnly;
				for (const auto& item : stock) msg.inventoryList.push_back({ item.lot, item.sortPriority });
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVendorStatusUpdate(&entity, a, stock, bUpdateOnly); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.bUpdateOnly, bUpdateOnly);
				EXPECT_EQ(copy.inventoryList, msg.inventoryList);
			}
		}
	}
}

TEST_F(VendorMessagesTests, VendorTransactionResultMatchesLegacy) {
	for (const auto target : g_Targets) {
		Entity entity(target, info);
		for (const auto result : { eVendorTransactionResult::SELL_SUCCESS, eVendorTransactionResult::SELL_FAIL, eVendorTransactionResult::PURCHASE_SUCCESS, eVendorTransactionResult::PURCHASE_FAIL, eVendorTransactionResult::DONATION_FAIL, eVendorTransactionResult::DONATION_FULL, static_cast<eVendorTransactionResult>(0xFFFFFFFF) }) {
			GameMessages::VendorTransactionResult msg;
			msg.target = target;
			msg.iResult = result;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendVendorTransactionResult(&entity, a, result); }, msg, SendMode::SendToClient);
			EXPECT_EQ(RoundTrip(msg).iResult, result);
		}
	}
}

TEST_F(VendorMessagesTests, TradeRepliesMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const auto other : g_Targets) {
			for (const auto& name : g_Names) {
				for (const bool flag : { false, true }) {
					GameMessages::ServerTradeInvite invite;
					invite.target = target;
					invite.bNeedInvitePopUp = flag;
					invite.i64Requestor = other;
					invite.wsName = name;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeInvite(target, flag, other, name, a); }, invite);
					const auto inviteCopy = RoundTrip(invite);
					EXPECT_EQ(inviteCopy.bNeedInvitePopUp, flag);
					EXPECT_EQ(inviteCopy.i64Requestor, other);
					EXPECT_EQ(inviteCopy.wsName, name);

					GameMessages::ServerTradeFinalReply finalReply;
					finalReply.target = target;
					finalReply.bResult = flag;
					finalReply.i64Invitee = other;
					finalReply.wsName = name;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeFinalReply(target, flag, other, name, a); }, finalReply);
					const auto finalCopy = RoundTrip(finalReply);
					EXPECT_EQ(finalCopy.bResult, flag);
					EXPECT_EQ(finalCopy.i64Invitee, other);
					EXPECT_EQ(finalCopy.wsName, name);
				}
				for (const auto resultType : g_Ints) {
					GameMessages::ServerTradeInitialReply initialReply;
					initialReply.target = target;
					initialReply.i64Invitee = other;
					initialReply.resultType = resultType;
					initialReply.wsName = name;
					ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeInitialReply(target, other, resultType, name, a); }, initialReply);
					const auto initialCopy = RoundTrip(initialReply);
					EXPECT_EQ(initialCopy.i64Invitee, other);
					EXPECT_EQ(initialCopy.resultType, resultType);
					EXPECT_EQ(initialCopy.wsName, name);
				}
			}
		}
	}
}

TEST_F(VendorMessagesTests, TradeAcceptAndCancelMatchLegacy) {
	for (const auto target : g_Targets) {
		for (const bool bFirst : { false, true }) {
			GameMessages::ServerTradeAccept accept;
			accept.target = target;
			accept.bFirst = bFirst;
			ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeAccept(target, bFirst, a); }, accept);
			EXPECT_EQ(RoundTrip(accept).bFirst, bFirst);
		}
		GameMessages::ServerTradeCancel cancel;
		cancel.target = target;
		ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeCancel(target, a); }, cancel);
		RoundTrip(cancel);
	}
}

TEST_F(VendorMessagesTests, ServerTradeUpdateMatchesLegacy) {
	std::vector<TradeItem> many;
	for (uint32_t i = 0; i < 50; i++) many.push_back({ 0x1000000000000000LL + i, static_cast<LOT>(i), i });
	const std::vector<std::vector<TradeItem>> itemSets = {
		{},
		{ { 0x0102030405060708LL, 1727, 1 } },
		{ { LWOOBJID_EMPTY, LOT_NULL, 0 }, { 0x1000000000000001LL, 0, std::numeric_limits<uint32_t>::max() } },
		many,
	};
	for (const auto target : g_Targets) {
		for (const uint64_t coins : { uint64_t{ 0 }, uint64_t{ 250 }, std::numeric_limits<uint64_t>::max() }) {
			for (const auto& items : itemSets) {
				GameMessages::ServerTradeUpdate msg;
				msg.target = target;
				msg.i64Currency = coins;
				for (const auto& item : items) msg.inventoryMap.push_back(EntryFor(item));
				ExpectSameAsLegacy([&](const SystemAddress& a) { LegacyGameMessages::SendServerTradeUpdate(target, coins, items, a); }, msg);
				const auto copy = RoundTrip(msg);
				EXPECT_EQ(copy.i64Currency, coins);
				EXPECT_EQ(copy.inventoryMap, msg.inventoryMap);
			}
		}
	}
}

TEST_F(VendorMessagesTests, VendorInboundReadsLikeLegacy) {
	for (const auto id : g_Targets) {
		for (const auto count : g_Ints) {
			for (const bool confirmed : { false, true }) {
				GameMessages::BuyFromVendor buy;
				buy.confirmed = confirmed;
				buy.count = count;
				buy.item = static_cast<LOT>(id);
				const auto legacyBuy = ReadWithLegacy<LegacyGameMessages::LegacyBuyFromVendor>(buy, LegacyGameMessages::ReadBuyFromVendor);
				const auto buyCopy = RoundTrip(buy);
				EXPECT_EQ(buyCopy.confirmed, legacyBuy.bConfirmed);
				EXPECT_EQ(buyCopy.count, legacyBuy.count);
				EXPECT_EQ(buyCopy.item, legacyBuy.item);
				ExpectTruncatedFails(buy);

				GameMessages::BuybackFromVendor buyback;
				buyback.confirmed = confirmed;
				buyback.count = count;
				buyback.item = id;
				const auto legacyBuyback = ReadWithLegacy<LegacyGameMessages::LegacyBuybackFromVendor>(buyback, LegacyGameMessages::ReadBuybackFromVendor);
				const auto buybackCopy = RoundTrip(buyback);
				EXPECT_EQ(buybackCopy.confirmed, legacyBuyback.confirmed);
				EXPECT_EQ(buybackCopy.count, legacyBuyback.count);
				EXPECT_EQ(buybackCopy.item, legacyBuyback.iObjID);
				ExpectTruncatedFails(buyback);

				GameMessages::RemoveDonationItem remove;
				remove.confirmed = confirmed;
				remove.count = static_cast<uint32_t>(count);
				remove.itemObjID = id;
				const auto legacyRemove = ReadWithLegacy<LegacyGameMessages::LegacyRemoveDonationItem>(remove, LegacyGameMessages::ReadRemoveDonationItem);
				const auto removeCopy = RoundTrip(remove);
				EXPECT_EQ(removeCopy.confirmed, legacyRemove.confirmed);
				EXPECT_EQ(removeCopy.count, legacyRemove.count);
				EXPECT_EQ(removeCopy.itemObjID, legacyRemove.itemId);
				ExpectTruncatedFails(remove);
			}

			GameMessages::SellToVendor sell;
			sell.count = count;
			sell.itemObjID = id;
			const auto legacySell = ReadWithLegacy<LegacyGameMessages::LegacySellToVendor>(sell, LegacyGameMessages::ReadSellToVendor);
			const auto sellCopy = RoundTrip(sell);
			EXPECT_EQ(sellCopy.count, legacySell.count);
			EXPECT_EQ(sellCopy.itemObjID, legacySell.iObjID);
			ExpectTruncatedFails(sell);

			GameMessages::AddDonationItem add;
			add.count = static_cast<uint32_t>(count);
			add.itemObjID = id;
			const auto legacyAdd = ReadWithLegacy<LegacyGameMessages::LegacyAddDonationItem>(add, LegacyGameMessages::ReadAddDonationItem);
			const auto addCopy = RoundTrip(add);
			EXPECT_EQ(addCopy.count, legacyAdd.count);
			EXPECT_EQ(addCopy.itemObjID, legacyAdd.itemId);
			ExpectTruncatedFails(add);
		}

		// The old handler read nothing, so ConfirmDonationOnPlayer is accepted with or without its vendorID.
		GameMessages::ConfirmDonationOnPlayer confirm;
		confirm.vendorID = id;
		EXPECT_EQ(RoundTrip(confirm).vendorID, id);
		RakNet::BitStream empty;
		GameMessages::ConfirmDonationOnPlayer fromEmpty;
		EXPECT_TRUE(fromEmpty.Deserialize(empty));
	}

	// Messages without a payload.
	RakNet::BitStream empty;
	EXPECT_TRUE(GameMessages::RequestVendorStatusUpdate().Deserialize(empty));
	EXPECT_TRUE(GameMessages::CancelDonationOnPlayer().Deserialize(empty));
	EXPECT_TRUE(GameMessages::ClientTradeCancel().Deserialize(empty));
}

TEST_F(VendorMessagesTests, TradeInboundReadsLikeLegacy) {
	for (const auto id : g_Targets) {
		for (const bool flag : { false, true }) {
			GameMessages::ClientTradeRequest request;
			request.bNeedInvitePopUp = flag;
			request.i64Invitee = id;
			const auto legacyRequest = ReadWithLegacy<LegacyGameMessages::LegacyClientTradeRequest>(request, LegacyGameMessages::ReadClientTradeRequest);
			const auto requestCopy = RoundTrip(request);
			EXPECT_EQ(requestCopy.bNeedInvitePopUp, legacyRequest.bNeedInvitePopUp);
			EXPECT_EQ(requestCopy.i64Invitee, legacyRequest.i64Invitee);
			ExpectTruncatedFails(request);

			GameMessages::ClientTradeAccept accept;
			accept.bFirst = flag;
			EXPECT_EQ(ReadWithLegacy<bool>(accept, LegacyGameMessages::ReadClientTradeAccept), flag);
			EXPECT_EQ(RoundTrip(accept).bFirst, flag);
			ExpectTruncatedFails(accept);
		}
	}

	// Every optional field of an entry, set and unset, with raw and compressed config blocks.
	std::vector<GameMessages::TradeItemEntry> entries;
	for (uint32_t mask = 0; mask < 64; mask++) {
		GameMessages::TradeItemEntry entry;
		entry.key = 0x0102030405060708LL + mask;
		entry.itemID = 0x1000000000000001LL + mask;
		entry.templateID = 1727 + static_cast<LOT>(mask);
		if (mask & 1) entry.subkey = 0x2000000000000002LL;
		if (mask & 2) entry.count = 7 + mask;
		if (mask & 4) entry.slot = static_cast<uint16_t>(3 + mask);
		if (mask & 8) entry.inventoryType = 5;
		if (mask & 16) {
			GameMessages::TradeItemEntry::Config config;
			config.compressed = (mask & 32) != 0;
			config.data = { 0xde, 0xad, 0xbe, 0xef, static_cast<uint8_t>(mask) };
			config.size = config.compressed ? 40 : static_cast<uint32_t>(config.data.size());
			config.compressedSize = config.compressed ? static_cast<uint32_t>(config.data.size()) : 0;
			entry.config = config;
		}
		entry.unknownFlag = (mask & 32) != 0;
		entries.push_back(entry);
	}
	const std::vector<std::vector<GameMessages::TradeItemEntry>> entrySets = { {}, { entries[0] }, { entries[3], entries[63] }, entries };

	for (const uint64_t currency : { uint64_t{ 0 }, uint64_t{ 99 }, std::numeric_limits<uint64_t>::max() }) {
		for (const auto& set : entrySets) {
			GameMessages::ClientTradeUpdate update;
			update.i64Currency = currency;
			update.inventoryMap = set;
			const auto legacy = ReadWithLegacy<LegacyGameMessages::LegacyClientTradeUpdate>(update, LegacyGameMessages::ReadClientTradeUpdate);
			const auto copy = RoundTrip(update);
			ASSERT_FALSE(legacy.rejected);
			EXPECT_EQ(copy.i64Currency, legacy.currency);
			ASSERT_EQ(copy.inventoryMap.size(), legacy.items.size());
			for (size_t i = 0; i < legacy.items.size(); i++) {
				const auto& entry = copy.inventoryMap[i];
				// What the handler hands to the trade.
				EXPECT_EQ(entry.key, legacy.items[i].itemId);
				EXPECT_EQ(entry.templateID, legacy.items[i].itemLot);
				EXPECT_EQ(entry.count.value_or(0), legacy.items[i].itemCount);
				// What it logs.
				EXPECT_EQ(entry.itemID, legacy.logs[i].itemId2);
				EXPECT_EQ(entry.subkey.value_or(0), legacy.logs[i].unknown1);
				EXPECT_EQ(entry.slot.value_or(0), legacy.logs[i].slot);
				EXPECT_EQ(entry.inventoryType.value_or(0), legacy.logs[i].unknown3);
				EXPECT_EQ(entry.unknownFlag, legacy.logs[i].unknown4);
			}
			if (set.size() <= 2) ExpectTruncatedFails(update);
		}
	}

	// The old handler ignored updates claiming more than MAX_MESSAGE_LENGTH items; the struct refuses them.
	RakNet::BitStream huge;
	huge.Write<uint64_t>(1);
	huge.Write<uint32_t>(MAX_MESSAGE_LENGTH + 1);
	RakNet::BitStream legacyHuge(huge.GetData(), huge.GetNumberOfBytesUsed(), false);
	EXPECT_TRUE(LegacyGameMessages::ReadClientTradeUpdate(legacyHuge).rejected);
	GameMessages::ClientTradeUpdate hugeUpdate;
	EXPECT_FALSE(hugeUpdate.Deserialize(huge));
}

// Independent of the legacy code: hand computed payloads (RakNet writes MSB first).
TEST_F(VendorMessagesTests, GoldenBytes) {
	GameMessages::BuyFromVendor buy;
	buy.confirmed = true;
	buy.item = 0x11;
	EXPECT_PACKET_EQ(FromHex("84 40 00 00 00", 34), Payload(buy));
	buy.confirmed = false;
	buy.count = 5;
	EXPECT_PACKET_EQ(FromHex("41 40 00 00 04 40 00 00 00", 66), Payload(buy));

	GameMessages::SellToVendor sell;
	sell.itemObjID = 0x33;
	EXPECT_PACKET_EQ(FromHex("19 80 00 00 00 00 00 00 00", 65), Payload(sell));

	GameMessages::VendorStatusUpdate status;
	status.bUpdateOnly = true;
	status.inventoryList = { { 1727, 0 }, { 3, -1 } };
	EXPECT_PACKET_EQ(FromHex("81 00 00 00 5f 83 00 00 00 00 00 00 01 80 00 00 7f ff ff ff 80", 161), Payload(status));

	GameMessages::VendorTransactionResult result;
	result.iResult = eVendorTransactionResult::PURCHASE_FAIL;
	EXPECT_PACKET_EQ(FromHex("03 00 00 00"), Payload(result));

	GameMessages::ServerTradeInvite invite;
	invite.i64Requestor = 0x11;
	invite.wsName = u"A";
	EXPECT_PACKET_EQ(FromHex("08 80 00 00 00 00 00 00 00 80 00 00 20 80 00", 113), Payload(invite));

	GameMessages::ServerTradeUpdate update;
	update.i64Currency = 250;
	update.inventoryMap.push_back(EntryFor({ 0x22, 1727, 3 }));
	EXPECT_PACKET_EQ(FromHex("7d 00 00 00 00 00 00 00 00 80 00 00 11 00 00 00 00 00 00 00 11 00 00 00 00 00 00 00 5f 83 00 00 20 60 00 00 00", 295), Payload(update));

	GameMessages::ServerTradeAccept accept;
	accept.bFirst = true;
	EXPECT_PACKET_EQ(FromHex("80", 1), Payload(accept));
}

// The whole packet as Send puts it on the wire, including the header, target and message ID.
TEST_F(VendorMessagesTests, GoldenPacket) {
	GameMessages::VendorOpenWindow open;
	open.target = 0x0102030405060708LL;
	// 53 = packet ID, 0005 = CLIENT, 0000000c = GAME_MSG, pad, target, u16 369.
	EXPECT_PACKET_EQ(FromHex("53 05 00 0c 00 00 00 00 08 07 06 05 04 03 02 01 71 01"), StructPacket(open));
}
