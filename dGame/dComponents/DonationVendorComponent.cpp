#include "DonationVendorComponent.h"
#include "Database.h"
#include "CharacterComponent.h"
#include "EconomyLedger.h"
#include "eInventoryType.h"
#include "eMissionTaskType.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "LeaderboardManager.h"
#include "Logger.h"
#include "MissionComponent.h"
#include "User.h"
#include "UserManager.h"

DonationVendorComponent::DonationVendorComponent(Entity* parent, const int32_t componentID) : VendorComponent(parent, componentID) {
	//LoadConfigData
	m_PercentComplete = 0.0;
	m_TotalDonated = 0;
	m_TotalRemaining = 0;

	// custom attribute to calculate other values
	m_Goal = m_Parent->GetVar<int32_t>(u"donationGoal");
	if (m_Goal == 0) m_Goal = INT32_MAX;

	// Default to the nexus tower jawbox activity and setup settings
	m_ActivityId = m_Parent->GetVar<uint32_t>(u"activityID");
	if ((m_ActivityId == 0) || (m_ActivityId == 117)) {
		m_ActivityId = 117;
		m_PercentComplete = 1.0;
		m_TotalDonated = INT32_MAX;
		m_TotalRemaining = 0;
		m_Goal = INT32_MAX;
		return;
	}

	auto donationTotal = Database::Get()->GetDonationTotal(m_ActivityId);
	if (donationTotal) m_TotalDonated = donationTotal.value();
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
}

void DonationVendorComponent::SubmitDonation(uint32_t count) {
	if (count <= 0 && ((m_TotalDonated + count) > 0)) return;
	m_TotalDonated += count;
	m_TotalRemaining = m_Goal - m_TotalDonated;
	m_PercentComplete = m_TotalDonated/static_cast<float>(m_Goal);
	m_DirtyDonationVendor = true;
}

void DonationVendorComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	VendorComponent::Serialize(outBitStream, bIsInitialUpdate);
	outBitStream.Write(bIsInitialUpdate || m_DirtyDonationVendor);
	if (bIsInitialUpdate || m_DirtyDonationVendor) {
		outBitStream.Write(m_PercentComplete);
		outBitStream.Write(m_TotalDonated);
		outBitStream.Write(m_TotalRemaining);
		if (!bIsInitialUpdate) m_DirtyDonationVendor = false;
	}
}

void DonationVendorComponent::AddDonationItem(const SystemAddress& sysAddr, LWOOBJID itemObjID, uint32_t count) {
	if (GetActivityID() == 0) {
		LOG("WARNING: Trying to dontate to a vendor with no activity");
		return;
	}
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) return;
	Entity* player = Game::entityManager->GetEntity(user->GetLoggedInChar());
	if (!player) return;
	auto* characterComponent = player->GetComponent<CharacterComponent>();
	if (!characterComponent) return;
	auto* inventoryComponent = player->GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;
	Item* item = inventoryComponent->FindItemById(itemObjID);
	if (!item) return;
	if (item->GetCount() < count) return;
	characterComponent->SetCurrentInteracting(m_Parent->GetObjectID());
	inventoryComponent->MoveItemToInventory(item, eInventoryType::DONATION, count, true, false, true);
}

void DonationVendorComponent::ConfirmDonation(Entity& player) {
	const auto [inventoryComponent, missionComponent, characterComponent] = player.GetComponentsMut<InventoryComponent, MissionComponent, CharacterComponent>();
	if (!inventoryComponent || !missionComponent || !characterComponent || !characterComponent->GetCurrentInteracting()) return;

	if (GetActivityID() == 0) {
		LOG("WARNING: Trying to dontate to a vendor with no activity");
		return;
	}
	auto* inventory = inventoryComponent->GetInventory(eInventoryType::DONATION);
	if (!inventory) return;
	auto items = inventory->GetItems();
	if (!items.empty()) {
		uint32_t count = 0;
		for (auto& [itemID, item] : items) {
			count += item->GetCount();
			EconomyLedger::RecordItemsUnsuppressed(&player, item->GetLot(), -static_cast<int64_t>(item->GetCount()), EconomyLedger::DONATION_SOURCE);
			item->RemoveFromInventory();
		}
		missionComponent->Progress(eMissionTaskType::DONATION, 0, LWOOBJID_EMPTY, "", count);
		LeaderboardManager::SaveScore(player.GetObjectID(), GetActivityID(), count);
		SubmitDonation(count);
		Game::entityManager->SerializeEntity(m_Parent);
	}
	characterComponent->SetCurrentInteracting(LWOOBJID_EMPTY);
}
