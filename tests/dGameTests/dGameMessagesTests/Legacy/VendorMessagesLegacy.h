#ifndef VENDORMESSAGESLEGACY_H
#define VENDORMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages::Send* functions that VendorMessages.h and TradeMessages.h
// replaced (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed,
// with one exception: SendVendorStatusUpdate takes the vendor's stock as a parameter instead of reading it from
// the entity's VendorComponent (a VendorComponent cannot be stocked without CDClient data in the tests); the
// rest of its body is unchanged.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions, verbatim up to
// the point where the handler starts using what it read.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Entity.h"
#include "eVendorTransactionResult.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ServiceType.h"
#include "TradingManager.h"
#include "VendorComponent.h"

#include <string>
#include <vector>

namespace LegacyGameMessages {
	inline void SendVendorOpenWindow(Entity* entity, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::VENDOR_OPEN_WINDOW);

		SEND_PACKET;
	}

	inline void SendVendorStatusUpdate(Entity* entity, const SystemAddress& sysAddr, const std::vector<SoldItem>& vendorItems, bool bUpdateOnly = false) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::VENDOR_STATUS_UPDATE);

		bitStream.Write(bUpdateOnly);
		bitStream.Write<uint32_t>(vendorItems.size());


		for (const auto& item : vendorItems) {
			bitStream.Write(item.lot);
			bitStream.Write(item.sortPriority);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendVendorTransactionResult(Entity* entity, const SystemAddress& sysAddr, eVendorTransactionResult result) {
		CBITSTREAM;
		CMSGHEADER;


		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::VENDOR_TRANSACTION_RESULT);
		bitStream.Write(result);

		SEND_PACKET;
	}

	inline void SendServerTradeInvite(LWOOBJID objectId, bool bNeedInvitePopUp, LWOOBJID i64Requestor, std::u16string wsName, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_INVITE);

		bitStream.Write(bNeedInvitePopUp);
		bitStream.Write(i64Requestor);
		bitStream.Write<uint32_t>(wsName.size());
		for (const auto character : wsName) {
			bitStream.Write(character);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendServerTradeInitialReply(LWOOBJID objectId, LWOOBJID i64Invitee, int32_t resultType, std::u16string wsName, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_INITIAL_REPLY);

		bitStream.Write(i64Invitee);
		bitStream.Write(resultType);
		bitStream.Write<uint32_t>(wsName.size());
		for (const auto character : wsName) {
			bitStream.Write(character);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendServerTradeFinalReply(LWOOBJID objectId, bool bResult, LWOOBJID i64Invitee, std::u16string wsName, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_FINAL_REPLY);

		bitStream.Write(bResult);
		bitStream.Write(i64Invitee);
		bitStream.Write<uint32_t>(wsName.size());
		for (const auto character : wsName) {
			bitStream.Write(character);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendServerTradeAccept(LWOOBJID objectId, bool bFirst, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_ACCEPT);

		bitStream.Write(bFirst);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendServerTradeCancel(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_CANCEL);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendServerTradeUpdate(LWOOBJID objectId, uint64_t coins, const std::vector<TradeItem>& items, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SERVER_TRADE_UPDATE);

		bitStream.Write(false);
		bitStream.Write(coins);
		bitStream.Write<uint32_t>(items.size());

		for (const auto& item : items) {
			bitStream.Write(item.itemId);
			bitStream.Write(item.itemId);

			bitStream.Write(item.itemLot);
			bitStream.Write0();
			bitStream.Write1();
			bitStream.Write(item.itemCount);
			bitStream.Write0();
			bitStream.Write0();
			bitStream.Write0();
			bitStream.Write0();
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	// GameMessages::HandleBuyFromVendor
	struct LegacyBuyFromVendor { bool bConfirmed{}; int count{}; LOT item{}; };
	inline LegacyBuyFromVendor ReadBuyFromVendor(RakNet::BitStream& inStream) {
		bool bConfirmed{}; // This doesn't appear to do anything.  Further research is needed.
		bool countIsDefault{};
		int count = 1;
		LOT item;

		inStream.Read(bConfirmed);
		inStream.Read(countIsDefault);
		if (countIsDefault) inStream.Read(count);
		inStream.Read(item);
		return { bConfirmed, count, item };
	}

	// GameMessages::HandleSellToVendor
	struct LegacySellToVendor { int count{}; LWOOBJID iObjID{}; };
	inline LegacySellToVendor ReadSellToVendor(RakNet::BitStream& inStream) {
		bool countIsDefault{};
		int count = 1;
		LWOOBJID iObjID;

		inStream.Read(countIsDefault);
		if (countIsDefault) inStream.Read(count);
		inStream.Read(iObjID);
		return { count, iObjID };
	}

	// GameMessages::HandleBuybackFromVendor
	struct LegacyBuybackFromVendor { bool confirmed{}; int count{}; LWOOBJID iObjID{}; };
	inline LegacyBuybackFromVendor ReadBuybackFromVendor(RakNet::BitStream& inStream) {
		bool confirmed = false;
		bool countIsDefault{};
		int count = 1;
		LWOOBJID iObjID;

		inStream.Read(confirmed);
		inStream.Read(countIsDefault);
		if (countIsDefault) inStream.Read(count);
		inStream.Read(iObjID);
		return { confirmed, count, iObjID };
	}

	// GameMessages::HandleAddDonationItem
	struct LegacyAddDonationItem { uint32_t count{}; LWOOBJID itemId{}; };
	inline LegacyAddDonationItem ReadAddDonationItem(RakNet::BitStream& inStream) {
		uint32_t count = 1;
		bool hasCount = false;
		inStream.Read(hasCount);
		if (hasCount) inStream.Read(count);
		LWOOBJID itemId = LWOOBJID_EMPTY;
		inStream.Read(itemId);
		return { count, itemId };
	}

	// GameMessages::HandleRemoveDonationItem
	struct LegacyRemoveDonationItem { bool confirmed{}; uint32_t count{}; LWOOBJID itemId{}; };
	inline LegacyRemoveDonationItem ReadRemoveDonationItem(RakNet::BitStream& inStream) {
		bool confirmed = false;
		inStream.Read(confirmed);
		uint32_t count = 1;
		bool hasCount = false;
		inStream.Read(hasCount);
		if (hasCount) inStream.Read(count);
		LWOOBJID itemId = LWOOBJID_EMPTY;
		inStream.Read(itemId);
		return { confirmed, count, itemId };
	}

	// GameMessages::HandleClientTradeRequest
	struct LegacyClientTradeRequest { bool bNeedInvitePopUp{}; LWOOBJID i64Invitee{}; };
	inline LegacyClientTradeRequest ReadClientTradeRequest(RakNet::BitStream& inStream) {
		bool bNeedInvitePopUp = inStream.ReadBit();
		LWOOBJID i64Invitee;

		inStream.Read(i64Invitee);
		return { bNeedInvitePopUp, i64Invitee };
	}

	// GameMessages::HandleClientTradeAccept
	inline bool ReadClientTradeAccept(RakNet::BitStream& inStream) {
		bool bFirst = inStream.ReadBit();
		return bFirst;
	}

	// GameMessages::HandleClientTradeUpdate. The per item log values are returned alongside the TradeItems.
	struct LegacyTradeItemLog { LWOOBJID itemId{}; LWOOBJID itemId2{}; LOT lot{}; LWOOBJID unknown1{}; uint32_t unknown2{}; uint16_t slot{}; uint32_t unknown3{}; bool unknown4{}; };
	struct LegacyClientTradeUpdate { bool rejected{}; uint64_t currency{}; std::vector<TradeItem> items{}; std::vector<LegacyTradeItemLog> logs{}; };
	inline LegacyClientTradeUpdate ReadClientTradeUpdate(RakNet::BitStream& inStream) {
		LegacyClientTradeUpdate result;
		uint64_t currency;
		uint32_t itemCount;

		inStream.Read(currency);
		inStream.Read(itemCount);
		if (itemCount > MAX_MESSAGE_LENGTH) { result.rejected = true; return result; }

		std::vector<TradeItem> items{};

		for (size_t i = 0; i < itemCount; i++) {
			LWOOBJID itemId;
			LWOOBJID itemId2;

			inStream.Read(itemId);
			inStream.Read(itemId2);

			LOT lot = 0;
			LWOOBJID unknown1 = 0;
			uint32_t unknown2 = 0;
			uint16_t slot = 0;
			uint32_t unknown3 = 0;
			uint32_t ldfSize = 0;
			bool unknown4;

			inStream.Read(lot);
			if (inStream.ReadBit()) {
				inStream.Read(unknown1);
			}
			if (inStream.ReadBit()) {
				inStream.Read(unknown2);
			}
			if (inStream.ReadBit()) {
				inStream.Read(slot);
			}
			if (inStream.ReadBit()) {
				inStream.Read(unknown3);
			}
			if (inStream.ReadBit()) // No
			{
				inStream.Read(ldfSize);
				bool compressed = inStream.ReadBit();
				if (compressed) {
					uint32_t ldfCompressedSize = 0;
					inStream.Read(ldfCompressedSize);
					inStream.IgnoreBytes(ldfCompressedSize);
				} else {
					inStream.IgnoreBytes(ldfSize);
				}
			}
			unknown4 = inStream.ReadBit();

			items.push_back({ itemId, lot, unknown2 });
			result.logs.push_back({ itemId, itemId2, lot, unknown1, unknown2, slot, unknown3, unknown4 });
		}
		result.currency = currency;
		result.items = items;
		return result;
	}
}

#endif // VENDORMESSAGESLEGACY_H
