#include "VendorComponent.h"
#include "BitStream.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "WorldConfig.h"
#include "CDComponentsRegistryTable.h"
#include "CDVendorComponentTable.h"
#include "CDLootMatrixTable.h"
#include "CDLootTableTable.h"
#include "CDItemComponentTable.h"
#include "InventoryComponent.h"
#include "Character.h"
#include "eVendorTransactionResult.h"
#include "UserManager.h"
#include "CheatDetection.h"

VendorComponent::VendorComponent(Entity* parent) : Component(parent) {
	m_HasStandardCostItems = false;
	m_HasMultiCostItems = false;
	SetupConstants();
	RefreshInventory(true);
}

void VendorComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write(bIsInitialUpdate || m_DirtyVendor);
	if (bIsInitialUpdate || m_DirtyVendor) {
		outBitStream.Write(m_HasStandardCostItems);
		outBitStream.Write(m_HasMultiCostItems);
		if (!bIsInitialUpdate) m_DirtyVendor = false;
	}
}

void VendorComponent::OnUse(Entity* originator) {
	GameMessages::SendVendorOpenWindow(m_Parent, originator->GetSystemAddress());
	GameMessages::SendVendorStatusUpdate(m_Parent, originator->GetSystemAddress());
}

void VendorComponent::RefreshInventory(bool isCreation) {
	SetHasStandardCostItems(false);
	SetHasMultiCostItems(false);
	m_Inventory.clear();

	// Custom code for Vanity Vendor Invetory Override
	if(m_Parent->HasVar(u"vendorInvOverride")) {
		std::vector<std::string> items = GeneralUtils::SplitString(m_Parent->GetVarAsString(u"vendorInvOverride"), ',');
		uint32_t sortPriority = -1;
		for (auto& itemString : items) {
			itemString.erase(remove_if(itemString.begin(), itemString.end(), isspace), itemString.end());
			auto item = GeneralUtils::TryParse<uint32_t>(itemString);
			if (!item) continue;
			if (SetupItem(item.value())) {
				sortPriority++;
				m_Inventory.push_back(SoldItem(item.value(), sortPriority));
			}
		}
		return;
	}

	auto* lootMatrixTable = CDClientManager::GetTable<CDLootMatrixTable>();
	const auto& lootMatrices = lootMatrixTable->GetMatrix(m_LootMatrixID);

	if (lootMatrices.empty()) return;

	auto* lootTableTable = CDClientManager::GetTable<CDLootTableTable>();

	for (const auto& lootMatrix : lootMatrices) {
		auto vendorItems = lootTableTable->GetTable(lootMatrix.LootTableIndex);
		if (lootMatrix.maxToDrop == 0 || lootMatrix.minToDrop == 0) {
			for (const auto& item : vendorItems) {
				if (SetupItem(item.itemid)) m_Inventory.push_back(SoldItem(item.itemid, item.sortPriority));
			}
		} else {
			auto randomCount = GeneralUtils::GenerateRandomNumber<int32_t>(lootMatrix.minToDrop, lootMatrix.maxToDrop);

			for (size_t i = 0; i < randomCount; i++) {
				if (vendorItems.empty()) break;
				auto randomItemIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, vendorItems.size() - 1);
				const auto& randomItem = vendorItems.at(randomItemIndex);
				vendorItems.erase(vendorItems.begin() + randomItemIndex);
				if (SetupItem(randomItem.itemid)) m_Inventory.push_back(SoldItem(randomItem.itemid, randomItem.sortPriority));
			}
		}
	}
	HandleMrReeCameras();

	// Callback timer to refresh this inventory.
	if (m_RefreshTimeSeconds > 0.0) {
		m_Parent->AddCallbackTimer(m_RefreshTimeSeconds, [this]() {
			RefreshInventory();
		});
	}
	Game::entityManager->SerializeEntity(m_Parent);
	GameMessages::SendVendorStatusUpdate(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);
}

void VendorComponent::SetupConstants() {
	auto* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	int componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), eReplicaComponentType::VENDOR);

	auto* vendorComponentTable = CDClientManager::GetTable<CDVendorComponentTable>();
	std::vector<CDVendorComponent> vendorComps = vendorComponentTable->Query([=](CDVendorComponent entry) { return (entry.id == componentID); });
	if (vendorComps.empty()) return;
	auto vendorData = vendorComps.at(0);
	if (vendorData.buyScalar == 0.0) m_BuyScalar = Game::zoneManager->GetWorldConfig()->vendorBuyMultiplier;
	else m_BuyScalar = vendorData.buyScalar;
	m_SellScalar = vendorData.sellScalar;
	m_RefreshTimeSeconds = vendorData.refreshTimeSeconds;
	m_LootMatrixID = vendorData.LootMatrixIndex;
}

bool VendorComponent::SellsItem(const LOT item) const {
	return std::count_if(m_Inventory.begin(), m_Inventory.end(), [item](const SoldItem& lhs) {
		return lhs.lot == item;
		}) > 0;
}

void VendorComponent::HandleMrReeCameras(){
	if (m_Parent->GetLOT() == 13569) {
		SetHasStandardCostItems(true);
		auto randomCamera = GeneralUtils::GenerateRandomNumber<int32_t>(0, 2);

		LOT camera = 0;
		DluAssert(randomCamera >= 0 && randomCamera <= 2);
		switch (randomCamera) {
		case 0:
			camera = 16253; // Grungagroid
			break;
		case 1:
			camera = 16254; // Hipstabrick
			break;
		case 2:
			camera = 16204; // Megabrixel snapshot
			break;
		}
		m_Inventory.push_back(SoldItem(camera, 0));
	}
}


void VendorComponent::Buy(Entity* buyer, LOT lot, uint32_t count) {

	if (!SellsItem(lot)) {
		auto* user = UserManager::Instance()->GetUser(buyer->GetSystemAddress());
		CheatDetection::ReportCheat(user, buyer->GetSystemAddress(), "Attempted to buy item %i from achievement vendor %i that is not purchasable", lot, m_Parent->GetLOT());
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	auto* inventoryComponent = buyer->GetComponent<InventoryComponent>();
	if (!inventoryComponent) {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();
	CDItemComponentTable* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	int itemCompID = compRegistryTable->GetByIDAndType(lot, eReplicaComponentType::ITEM);
	CDItemComponent itemComp = itemComponentTable->GetItemComponentByID(itemCompID);

	// Extra currency that needs to be deducted in case of crafting
	auto craftingCurrencies = CDItemComponentTable::ParseCraftingCurrencies(itemComp);
	for (const auto& [crafintCurrencyLOT, crafintCurrencyCount]: craftingCurrencies) {
		if (inventoryComponent->GetLotCount(crafintCurrencyLOT) < (crafintCurrencyCount * count)) {
			GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
			return;
		}
	}
	for (const auto& [crafintCurrencyLOT, crafintCurrencyCount]: craftingCurrencies) {
		inventoryComponent->RemoveItem(crafintCurrencyLOT, crafintCurrencyCount * count);
	}


	float buyScalar = GetBuyScalar();
	const auto coinCost = static_cast<uint32_t>(std::floor((itemComp.baseValue * buyScalar) * count));

	Character* character = buyer->GetCharacter();
	if (!character || character->GetCoins() < coinCost) {
		GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
		return;
	}

	if (Inventory::IsValidItem(itemComp.currencyLOT)) {
		const uint32_t altCurrencyCost = std::floor(itemComp.altCurrencyCost * buyScalar) * count;
		if (inventoryComponent->GetLotCount(itemComp.currencyLOT) < altCurrencyCost) {
			GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_FAIL);
			return;
		}
		inventoryComponent->RemoveItem(itemComp.currencyLOT, altCurrencyCost);
	}

	character->SetCoins(character->GetCoins() - (coinCost), eLootSourceType::VENDOR);
	inventoryComponent->AddItem(lot, count, eLootSourceType::VENDOR);
	GameMessages::SendVendorTransactionResult(buyer, buyer->GetSystemAddress(), eVendorTransactionResult::PURCHASE_SUCCESS);
}

bool VendorComponent::SetupItem(LOT item) {

	auto* itemComponentTable = CDClientManager::GetTable<CDItemComponentTable>();
	auto* compRegistryTable = CDClientManager::GetTable<CDComponentsRegistryTable>();

	auto itemComponentID = compRegistryTable->GetByIDAndType(item, eReplicaComponentType::ITEM, -1);
	if (itemComponentID == -1) {
		LOG("Attempted to add item %i with ItemComponent ID -1 to vendor %i inventory. Not adding item!", itemComponentID, m_Parent->GetLOT());
		return false;
	}

	if (!m_HasStandardCostItems || !m_HasMultiCostItems) {
		auto itemComponent = itemComponentTable->GetItemComponentByID(itemComponentID);
		if (!m_HasStandardCostItems && itemComponent.baseValue != -1) SetHasStandardCostItems(true);
		if (!m_HasMultiCostItems && !itemComponent.currencyCosts.empty()) SetHasMultiCostItems(true);
	}

	return true;
}

