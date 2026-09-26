#ifndef VENDORMESSAGES_H
#define VENDORMESSAGES_H

#include "GameMessages.h"

#include <vector>

enum class eVendorTransactionResult : uint32_t;

// Game messages for vendors (buy, sell, buyback, the vendor window and its stock) and donation vendors.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
namespace GameMessages {
	// Server -> client. Sent with SendToClient: the old function never broadcast.
	struct VendorOpenWindow : public NetGameMsg {
		VendorOpenWindow() : NetGameMsg(MessageType::Game::VENDOR_OPEN_WINDOW) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};

	// Client -> server. Asks the vendor (the target) to resend its stock. No payload.
	struct RequestVendorStatusUpdate : public NetGameMsg {
		RequestVendorStatusUpdate() : NetGameMsg(MessageType::Game::REQUEST_VENDOR_STATUS_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client. The vendor's stock. UNASSIGNED_SYSTEM_ADDRESS broadcasts.
	struct VendorStatusUpdate : public NetGameMsg {
		VendorStatusUpdate() : NetGameMsg(MessageType::Game::VENDOR_STATUS_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		struct Entry {
			LOT lot{};
			int32_t sortPriority{};
			bool operator==(const Entry&) const = default;
		};

		bool bUpdateOnly{};
		std::vector<Entry> inventoryList{}; // u32 count, then the entries
	};

	// Server -> client. Sent with SendToClient: the old function never broadcast.
	struct VendorTransactionResult : public NetGameMsg {
		VendorTransactionResult() : NetGameMsg(MessageType::Game::VENDOR_TRANSACTION_RESULT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eVendorTransactionResult iResult{};
	};

	// Client -> server. Target is the vendor.
	struct BuyFromVendor : public NetGameMsg {
		BuyFromVendor() : NetGameMsg(MessageType::Game::BUY_FROM_VENDOR) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool confirmed{};
		int32_t count{ 1 }; // optional
		LOT item{};
	};

	// Client -> server. Target is the vendor.
	struct SellToVendor : public NetGameMsg {
		SellToVendor() : NetGameMsg(MessageType::Game::SELL_TO_VENDOR) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t count{ 1 }; // optional
		LWOOBJID itemObjID{};
	};

	// Client -> server. Target is the vendor.
	struct BuybackFromVendor : public NetGameMsg {
		BuybackFromVendor() : NetGameMsg(MessageType::Game::BUYBACK_FROM_VENDOR) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool confirmed{};
		int32_t count{ 1 }; // optional
		LWOOBJID item{};
	};

	// Client -> server. Target is the donation vendor.
	struct AddDonationItem : public NetGameMsg {
		AddDonationItem() : NetGameMsg(MessageType::Game::ADD_DONATION_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		uint32_t count{ 1 }; // optional
		LWOOBJID itemObjID{};
	};

	// Client -> server.
	struct RemoveDonationItem : public NetGameMsg {
		RemoveDonationItem() : NetGameMsg(MessageType::Game::REMOVE_DONATION_ITEM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool confirmed{};
		uint32_t count{ 1 }; // optional
		LWOOBJID itemObjID{};
	};

	// Client -> server. Target is the player. The vendor comes from the player's current interaction,
	// not from vendorID.
	struct ConfirmDonationOnPlayer : public NetGameMsg {
		ConfirmDonationOnPlayer() : NetGameMsg(MessageType::Game::CONFIRM_DONATION_ON_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		// Not read by DLU: the old handler ignored the payload, so a message without it is still accepted.
		LWOOBJID vendorID{};
	};

	// Client -> server. Target is the player. No payload.
	struct CancelDonationOnPlayer : public NetGameMsg {
		CancelDonationOnPlayer() : NetGameMsg(MessageType::Game::CANCEL_DONATION_ON_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};
};

#endif // VENDORMESSAGES_H
