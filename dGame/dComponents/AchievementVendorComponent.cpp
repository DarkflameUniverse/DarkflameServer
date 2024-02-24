#include "AchievementVendorComponent.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"
#include "eMissionState.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "eVendorTransactionResult.h"


bool AchievementVendorComponent::SellsItem(const LOT item) const {
	// TODO: Logic for checking if player has completed an achievement to be able to by the lot
	return true;
}
void AchievementVendorComponent::Buy(Entity* buyer, LOT lot, uint32_t count) {
	if (SellsItem(lot)) {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	auto* inventoryComponent = buyer->GetComponent<InventoryComponent>();
	if (!inventoryComponent)  {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	// get the item Comp from the item LOT
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	int itemCompID = compRegistryTable->GetByIDAndType(lot, eReplicaComponentType::ITEM);
	CDItemComponent itemComp = itemComponentTable->GetItemComponentByID(itemCompID);
	uint32_t costLOT = itemComp.commendationLOT;

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