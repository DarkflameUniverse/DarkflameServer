#include "VendorComponent.h"

#include <BitStream.h>

#include "Game.h"
#include "dServer.h"

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

void VendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyVendor);
	if (bIsInitialUpdate || m_DirtyVendor) {
		outBitStream->Write(m_HasStandardCostItems);
		outBitStream->Write(m_HasMultiCostItems);
	}
}

void VendorComponent::OnUse(Entity* originator) {
	GameMessages::SendVendorOpenWindow(m_ParentEntity, originator->GetSystemAddress());
	GameMessages::SendVendorStatusUpdate(m_ParentEntity, originator->GetSystemAddress());
}


void VendorComponent::RefreshInventory(bool isCreation) {
	SetHasStandardCostItems(false);
	SetHasMultiCostItems(false);

	//Custom code for Max vanity NPC
	if (m_ParentEntity->GetLOT() == 9749 && Game::server->GetZoneID() == 1201) {
		if (!isCreation) return;
		SetHasStandardCostItems(true);
		m_Inventory.insert({ 11909, 0 }); // Top hat w frog
		m_Inventory.insert({ 7785, 0 });  // Flash bulb
		m_Inventory.insert({ 12764, 0 }); // Big fountain soda
		m_Inventory.insert({ 12241, 0 }); // Hot cocoa (from fb)
		return;
	}
	m_Inventory.clear();
	auto* lootMatrixTable = CDClientManager::Instance().GetTable<CDLootMatrixTable>();
	std::vector<CDLootMatrix> lootMatrices = lootMatrixTable->Query([=](CDLootMatrix entry) { return (entry.LootMatrixIndex == m_LootMatrixID); });

	if (lootMatrices.empty()) return;
	// Done with lootMatrix table

	auto* lootTableTable = CDClientManager::Instance().GetTable<CDLootTableTable>();
	auto* itemComponentTable = CDClientManager::Instance().GetTable<CDItemComponentTable>();
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	for (const auto& lootMatrix : lootMatrices) {
		int lootTableID = lootMatrix.LootTableIndex;
		std::vector<CDLootTable> vendorItems = lootTableTable->Query([=](CDLootTable entry) { return (entry.LootTableIndex == lootTableID); });
		if (lootMatrix.maxToDrop == 0 || lootMatrix.minToDrop == 0) {
			for (CDLootTable item : vendorItems) {
				if (!m_HasStandardCostItems || !m_HasMultiCostItems){
					auto itemComponentID = compRegistryTable->GetByIDAndType(item.itemid, eReplicaComponentType::ITEM);
					auto itemComponent = itemComponentTable->GetItemComponentByID(itemComponentID);
					if (!m_HasStandardCostItems && itemComponent.baseValue != -1) SetHasStandardCostItems(true);
					if (!m_HasMultiCostItems && !itemComponent.currencyCosts.empty()) SetHasMultiCostItems(true);
				}
				m_Inventory.insert({ item.itemid, item.sortPriority });
			}
		} else {
			auto randomCount = GeneralUtils::GenerateRandomNumber<int32_t>(lootMatrix.minToDrop, lootMatrix.maxToDrop);

			for (size_t i = 0; i < randomCount; i++) {
				if (vendorItems.empty()) break;
				auto randomItemIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, vendorItems.size() - 1);
				const auto& randomItem = vendorItems[randomItemIndex];
				vendorItems.erase(vendorItems.begin() + randomItemIndex);
				if (!m_HasStandardCostItems || !m_HasMultiCostItems){
					auto itemComponentID = compRegistryTable->GetByIDAndType(randomItem.itemid, eReplicaComponentType::ITEM);
					auto itemComponent = itemComponentTable->GetItemComponentByID(itemComponentID);
					if (!m_HasStandardCostItems && itemComponent.baseValue != -1) SetHasStandardCostItems(true);
					if (!m_HasMultiCostItems && !itemComponent.currencyCosts.empty()) SetHasMultiCostItems(true);
				}
				m_Inventory.insert({ randomItem.itemid, randomItem.sortPriority });
			}
		}
	}

	//Because I (Max) want a vendor to sell these cameras
	if (m_ParentEntity->GetLOT() == 13569) {
		auto randomCamera = GeneralUtils::GenerateRandomNumber<int32_t>(0, 2);

		switch (randomCamera) {
		case 0:
			m_Inventory.insert({ 16253, 0 }); //Grungagroid
			break;
		case 1:
			m_Inventory.insert({ 16254, 0 }); //Hipstabrick
			break;
		case 2:
			m_Inventory.insert({ 16204, 0 }); //Megabrixel snapshot
			break;
		default:
			break;
		}
	}

	// Callback timer to refresh this inventory.
	m_ParentEntity->AddCallbackTimer(m_RefreshTimeSeconds, [this]() {
		RefreshInventory();
		}
	);
	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	GameMessages::SendVendorStatusUpdate(m_ParentEntity, UNASSIGNED_SYSTEM_ADDRESS);
}

void VendorComponent::SetupConstants() {
	auto* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	int componentID = compRegistryTable->GetByIDAndType(m_ParentEntity->GetLOT(), eReplicaComponentType::VENDOR);

	auto* vendorComponentTable = CDClientManager::Instance().GetTable<CDVendorComponentTable>();
	std::vector<CDVendorComponent> vendorComps = vendorComponentTable->Query([=](CDVendorComponent entry) { return (entry.id == componentID); });
	if (vendorComps.empty()) return;
	m_BuyScalar = vendorComps[0].buyScalar;
	m_SellScalar = vendorComps[0].sellScalar;
	m_RefreshTimeSeconds = vendorComps[0].refreshTimeSeconds;
	m_LootMatrixID = vendorComps[0].LootMatrixIndex;
}


