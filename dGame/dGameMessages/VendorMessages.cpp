#include "VendorMessages.h"

#include "AchievementVendorComponent.h"
#include "BitStreamUtils.h"
#include "CharacterComponent.h"
#include "DonationVendorComponent.h"
#include "eInventoryType.h"
#include "eReplicaComponentType.h"
#include "eVendorTransactionResult.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "PropertyVendorComponent.h"
#include "User.h"
#include "UserManager.h"
#include "VendorComponent.h"

namespace {
	// The player that sent a message, found from the sender's address as the old handlers did.
	Entity* GetSendingPlayer(const SystemAddress& sysAddr) {
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) return nullptr;
		return Game::entityManager->GetEntity(user->GetLoggedInChar());
	}
}

namespace GameMessages {
	void VendorOpenWindow::Serialize(RakNet::BitStream& bitStream) const {}

	bool VendorOpenWindow::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void RequestVendorStatusUpdate::Serialize(RakNet::BitStream& bitStream) const {}

	bool RequestVendorStatusUpdate::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void RequestVendorStatusUpdate::Handle(Entity& entity, const SystemAddress& sysAddr) {
		// Only the VENDOR component answers, as before (not achievement or donation vendors).
		auto* vendor = static_cast<VendorComponent*>(entity.GetComponent(eReplicaComponentType::VENDOR));
		if (vendor) vendor->SendStatusUpdate(sysAddr, true);
	}

	void VendorStatusUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bUpdateOnly);
		bitStream.Write<uint32_t>(inventoryList.size());
		for (const auto& entry : inventoryList) {
			bitStream.Write(entry.lot);
			bitStream.Write(entry.sortPriority);
		}
	}

	bool VendorStatusUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bUpdateOnly));
		uint32_t count{};
		VALIDATE_READ(bitStream.Read(count));
		// Each entry is 8 bytes; refuse counts the stream cannot hold before allocating.
		if (static_cast<uint64_t>(count) * 64 > bitStream.GetNumberOfUnreadBits()) return false;
		inventoryList.resize(count);
		for (auto& entry : inventoryList) {
			VALIDATE_READ(bitStream.Read(entry.lot));
			VALIDATE_READ(bitStream.Read(entry.sortPriority));
		}
		return true;
	}

	void VendorTransactionResult::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(iResult);
	}

	bool VendorTransactionResult::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(iResult));
		return true;
	}

	void BuyFromVendor::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(confirmed);
		BitStreamUtils::WriteOptional(bitStream, count, 1);
		bitStream.Write(item);
	}

	bool BuyFromVendor::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(confirmed));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, count, 1));
		VALIDATE_READ(bitStream.Read(item));
		return true;
	}

	void BuyFromVendor::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Entity* player = GetSendingPlayer(sysAddr);
		if (!player) return;

		// handle buying normal items
		auto* vendorComponent = entity.GetComponent<VendorComponent>();
		if (vendorComponent) {
			vendorComponent->Buy(player, item, count);
			return;
		}

		// handle buying achievement items
		auto* achievementVendorComponent = entity.GetComponent<AchievementVendorComponent>();
		if (achievementVendorComponent) {
			achievementVendorComponent->Buy(player, item, count);
			return;
		}

		// Handle buying properties
		auto* propertyVendorComponent = entity.GetComponent<PropertyVendorComponent>();
		if (propertyVendorComponent) {
			propertyVendorComponent->OnBuyFromVendor(player, confirmed, item, count);
			return;
		}
	}

	void SellToVendor::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, count, 1);
		bitStream.Write(itemObjID);
	}

	bool SellToVendor::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, count, 1));
		VALIDATE_READ(bitStream.Read(itemObjID));
		return true;
	}

	void SellToVendor::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Entity* player = GetSendingPlayer(sysAddr);
		if (!player) return;

		auto* vendor = static_cast<VendorComponent*>(entity.GetComponent(eReplicaComponentType::VENDOR));
		if (!vendor) return;

		vendor->SellToVendor(*player, sysAddr, itemObjID, count);
	}

	void BuybackFromVendor::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(confirmed);
		BitStreamUtils::WriteOptional(bitStream, count, 1);
		bitStream.Write(item);
	}

	bool BuybackFromVendor::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(confirmed));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, count, 1));
		VALIDATE_READ(bitStream.Read(item));
		return true;
	}

	void BuybackFromVendor::Handle(Entity& entity, const SystemAddress& sysAddr) {
		//if (!confirmed) return; they always built in this confirmed garbage... but never used it?

		Entity* player = GetSendingPlayer(sysAddr);
		if (!player) return;

		auto* vendor = static_cast<VendorComponent*>(entity.GetComponent(eReplicaComponentType::VENDOR));
		if (!vendor) return;

		vendor->BuybackFromVendor(*player, sysAddr, item, count);
	}

	void AddDonationItem::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, count, 1u);
		bitStream.Write(itemObjID);
	}

	bool AddDonationItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, count, 1u));
		VALIDATE_READ(bitStream.Read(itemObjID));
		return true;
	}

	void AddDonationItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!itemObjID) return;

		auto* donationVendorComponent = entity.GetComponent<DonationVendorComponent>();
		if (!donationVendorComponent) return;
		donationVendorComponent->AddDonationItem(sysAddr, itemObjID, count);
	}

	void RemoveDonationItem::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(confirmed);
		BitStreamUtils::WriteOptional(bitStream, count, 1u);
		bitStream.Write(itemObjID);
	}

	bool RemoveDonationItem::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(confirmed));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, count, 1u));
		VALIDATE_READ(bitStream.Read(itemObjID));
		return true;
	}

	void RemoveDonationItem::Handle(Entity& entity, const SystemAddress& sysAddr) {
		if (!itemObjID) return;

		Entity* player = GetSendingPlayer(sysAddr);
		if (!player) return;

		auto* inventoryComponent = player->GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;

		Item* item = inventoryComponent->FindItemById(itemObjID);
		if (!item) return;
		if (item->GetCount() < count) return;
		inventoryComponent->MoveItemToInventory(item, eInventoryType::BRICKS, count, true, false, true);
	}

	void ConfirmDonationOnPlayer::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(vendorID);
	}

	bool ConfirmDonationOnPlayer::Deserialize(RakNet::BitStream& bitStream) {
		// The old handler read nothing; keep accepting the message even if vendorID is missing.
		if (!bitStream.Read(vendorID)) vendorID = LWOOBJID_EMPTY;
		return true;
	}

	void ConfirmDonationOnPlayer::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* characterComponent = entity.GetComponent<CharacterComponent>();
		if (!characterComponent || !characterComponent->GetCurrentInteracting()) return;

		auto* donationEntity = Game::entityManager->GetEntity(characterComponent->GetCurrentInteracting());
		if (!donationEntity) return;
		auto* donationVendorComponent = donationEntity->GetComponent<DonationVendorComponent>();
		if (!donationVendorComponent) return;
		donationVendorComponent->ConfirmDonation(entity);
	}

	void CancelDonationOnPlayer::Serialize(RakNet::BitStream& bitStream) const {}

	bool CancelDonationOnPlayer::Deserialize(RakNet::BitStream& bitStream) {
		return true;
	}

	void CancelDonationOnPlayer::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* inventoryComponent = entity.GetComponent<InventoryComponent>();
		if (!inventoryComponent) return;
		auto* inventory = inventoryComponent->GetInventory(eInventoryType::DONATION);
		if (!inventory) return;
		auto items = inventory->GetItems();
		for (auto& [itemID, item] : items) {
			inventoryComponent->MoveItemToInventory(item, eInventoryType::BRICKS, item->GetCount(), false, false, true);
		}
		auto* characterComponent = entity.GetComponent<CharacterComponent>();
		if (!characterComponent) return;
		characterComponent->SetCurrentInteracting(LWOOBJID_EMPTY);
	}
}
