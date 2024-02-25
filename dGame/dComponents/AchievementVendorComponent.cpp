#include "AchievementVendorComponent.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"
#include "eMissionState.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "eVendorTransactionResult.h"
#include "CheatDetection.h"
#include "UserManager.h"
#include "CDMissionsTable.h"

bool AchievementVendorComponent::SellsItem(Entity* buyer, const LOT lot) {
	auto* missionComponent = buyer->GetComponent<MissionComponent>();
	if (!missionComponent) return false;

	if (m_PlayerPurchasableItems[buyer->GetObjectID()].contains(lot)){
		return true;
	}

	CDMissionsTable* missionsTable = CDClientManager::GetTable<CDMissionsTable>();
	const auto missions = missionsTable->GetMissionsForReward(lot);
	for (const auto mission : missions) {
		if (missionComponent->GetMissionState(mission) == eMissionState::COMPLETE) {
			m_PlayerPurchasableItems[buyer->GetObjectID()].insert(lot);
			return true;
		}
	}
	return false;
}

void AchievementVendorComponent::Buy(Entity* buyer, LOT lot, uint32_t count) {
	// get the item Comp from the item LOT
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	int itemCompID = compRegistryTable->GetByIDAndType(lot, eReplicaComponentType::ITEM);
	CDItemComponent itemComp = itemComponentTable->GetItemComponentByID(itemCompID);
	uint32_t costLOT = itemComp.commendationLOT;
	
	if (costLOT == -1 || !SellsItem(buyer, lot)) {
		auto* user = UserManager::Instance()->GetUser(buyer->GetSystemAddress());
		CheatDetection::ReportCheat(user, buyer->GetSystemAddress(), "Attempted to buy item %i from achievement vendor %i that is not purchasable", lot, m_Parent->GetLOT());
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	auto* inventoryComponent = buyer->GetComponent<InventoryComponent>();
	if (!inventoryComponent)  {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	if (costLOT == 13763) { // Faction Token Proxy
		auto* missionComponent = buyer->GetComponent<MissionComponent>();
		if (!missionComponent) return;

		if (missionComponent->GetMissionState(545) == eMissionState::COMPLETE) costLOT = 8318; // "Assembly Token"
		if (missionComponent->GetMissionState(556) == eMissionState::COMPLETE) costLOT = 8321; // "Venture League Token"
		if (missionComponent->GetMissionState(567) == eMissionState::COMPLETE) costLOT = 8319; // "Sentinels Token"
		if (missionComponent->GetMissionState(578) == eMissionState::COMPLETE) costLOT = 8320; // "Paradox Token"
	}

	const uint32_t altCurrencyCost = itemComp.commendationCost * count;
	if (inventoryComponent->GetLotCount(costLOT) < altCurrencyCost) {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	inventoryComponent->RemoveItem(costLOT, altCurrencyCost);
	inventoryComponent->AddItem(lot, count, eLootSourceType::VENDOR);
	GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_SUCCESS);

}