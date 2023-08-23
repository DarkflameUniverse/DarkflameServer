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

VendorComponent::VendorComponent(Entity* parent) : Component(parent) {
	m_HasStandardCostItems = false;
	m_HasMultiCostItems = false;
	SetupConstants();
	RefreshInventory(true);
}

void VendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyVendor);
	if (bIsInitialUpdate || m_DirtyVendor) {
		outBitStream->Write(m_HasStandardCostItems);
		outBitStream->Write(m_HasMultiCostItems);
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

	// Custom code for Max vanity NPC and Mr.Ree cameras
	if(isCreation && m_Parent->GetLOT() == 9749 && Game::server->GetZoneID() == 1201) {
		SetupMaxCustomVendor();
		return;
	}

	auto* lootMatrixTable = CDClientManager::Instance().GetTable<CDLootMatrixTable>();
	const auto lootMatrices = lootMatrixTable->Query([=](CDLootMatrix entry) { return (entry.LootMatrixIndex == m_LootMatrixID); });

	if (lootMatrices.empty()) return;

	auto* lootTableTable = CDClientManager::Instance().GetTable<CDLootTableTable>();
	auto* itemComponentTable = CDClientManager::Instance().GetTable<CDItemComponentTable>();
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	for (const auto& lootMatrix : lootMatrices) {
		int lootTableID = lootMatrix.LootTableIndex;
		auto vendorItems = lootTableTable->Query([=](CDLootTable entry) { return (entry.LootTableIndex == lootTableID); });
		if (lootMatrix.maxToDrop == 0 || lootMatrix.minToDrop == 0) {
			for (const auto& item : vendorItems) {
				if (!m_HasStandardCostItems || !m_HasMultiCostItems) {
					auto itemComponentID = compRegistryTable->GetByIDAndType(item.itemid, eReplicaComponentType::ITEM, -1);
					if (itemComponentID == -1) {
						Game::logger->Log("VendorComponent", "Attempted to add item %i with ItemComponent ID -1 to vendor %i inventory. Not adding item!", itemComponentID, m_Parent->GetLOT());
						continue;
					}
					auto itemComponent = itemComponentTable->GetItemComponentByID(itemComponentID);
					if (!m_HasStandardCostItems && itemComponent.baseValue != -1) SetHasStandardCostItems(true);
					if (!m_HasMultiCostItems && !itemComponent.currencyCosts.empty()) SetHasMultiCostItems(true);
				}
				m_Inventory.push_back(SoldItem(item.itemid, item.sortPriority));
			}
		} else {
			auto randomCount = GeneralUtils::GenerateRandomNumber<int32_t>(lootMatrix.minToDrop, lootMatrix.maxToDrop);

			for (size_t i = 0; i < randomCount; i++) {
				if (vendorItems.empty()) break;
				auto randomItemIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, vendorItems.size() - 1);
				const auto& randomItem = vendorItems.at(randomItemIndex);
				vendorItems.erase(vendorItems.begin() + randomItemIndex);
				if (!m_HasStandardCostItems || !m_HasMultiCostItems) {
					auto itemComponentID = compRegistryTable->GetByIDAndType(randomItem.itemid, eReplicaComponentType::ITEM, -1);
					if (itemComponentID == -1) {
						Game::logger->Log("VendorComponent", "Attempted to add item %i with ItemComponent ID -1 to vendor %i inventory. Not adding item!", itemComponentID, m_Parent->GetLOT());
						continue;
					}
					auto itemComponent = itemComponentTable->GetItemComponentByID(itemComponentID);
					if (!m_HasStandardCostItems && itemComponent.baseValue != -1) SetHasStandardCostItems(true);
					if (!m_HasMultiCostItems && !itemComponent.currencyCosts.empty()) SetHasMultiCostItems(true);
				}
				m_Inventory.push_back(SoldItem(randomItem.itemid, randomItem.sortPriority));
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
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	int componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), eReplicaComponentType::VENDOR);

	auto* vendorComponentTable = CDClientManager::Instance().GetTable<CDVendorComponentTable>();
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


void VendorComponent::SetupMaxCustomVendor(){
	SetHasStandardCostItems(true);
	m_Inventory.push_back(SoldItem(11909, 0)); // Top hat w frog
	m_Inventory.push_back(SoldItem(7785, 0));  // Flash bulb
	m_Inventory.push_back(SoldItem(12764, 0)); // Big fountain soda
	m_Inventory.push_back(SoldItem(12241, 0)); // Hot cocoa (from fb)
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
