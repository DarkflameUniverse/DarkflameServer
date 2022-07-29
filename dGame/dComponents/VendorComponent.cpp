#include "VendorComponent.h"

#include <BitStream.h>

#include "Game.h"
#include "dServer.h"

VendorComponent::VendorComponent(Entity* parent) : Component(parent) {
	SetupConstants();
	RefreshInventory(true);
}

VendorComponent::~VendorComponent() = default;

void VendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write1();
	outBitStream->Write1(); // Has standard items (Required for vendors with missions.)
	outBitStream->Write(HasCraftingStation()); // Has multi use items
}

void VendorComponent::OnUse(Entity* originator) {
	GameMessages::SendVendorOpenWindow(m_Parent, originator->GetSystemAddress());
	GameMessages::SendVendorStatusUpdate(m_Parent, originator->GetSystemAddress());
}

float VendorComponent::GetBuyScalar() const {
	return m_BuyScalar;
}

float VendorComponent::GetSellScalar() const {
	return m_SellScalar;
}

void VendorComponent::SetBuyScalar(float value) {
	m_BuyScalar = value;
}

void VendorComponent::SetSellScalar(float value) {
	m_SellScalar = value;
}

std::map<LOT, int>& VendorComponent::GetInventory() {
	return m_Inventory;
}

bool VendorComponent::HasCraftingStation() {
	// As far as we know, only Umami has a crafting station
	return m_Parent->GetLOT() == 13800;
}

void VendorComponent::RefreshInventory(bool isCreation) {
	//Custom code for Max vanity NPC
	if (m_Parent->GetLOT() == 9749 && Game::server->GetZoneID() == 1201) {
		if (!isCreation) return;
		m_Inventory.insert({ 11909, 0 }); //Top hat w frog
		m_Inventory.insert({ 7785, 0 }); //Flash bulb
		m_Inventory.insert({ 12764, 0 }); //Big fountain soda
		m_Inventory.insert({ 12241, 0 }); //Hot cocoa (from fb)
		return;
	}
	m_Inventory.clear();
	auto* lootMatrixTable = CDClientManager::Instance()->GetTable<CDLootMatrixTable>("LootMatrix");
	std::vector<CDLootMatrix> lootMatrices = lootMatrixTable->Query([=](CDLootMatrix entry) { return (entry.LootMatrixIndex == m_LootMatrixID); });

	if (lootMatrices.empty()) return;
	// Done with lootMatrix table

	auto* lootTableTable = CDClientManager::Instance()->GetTable<CDLootTableTable>("LootTable");

	for (const auto& lootMatrix : lootMatrices) {
		int lootTableID = lootMatrix.LootTableIndex;
		std::vector<CDLootTable> vendorItems = lootTableTable->Query([=](CDLootTable entry) { return (entry.LootTableIndex == lootTableID); });
		if (lootMatrix.maxToDrop == 0 || lootMatrix.minToDrop == 0) {
			for (CDLootTable item : vendorItems) {
				m_Inventory.insert({ item.itemid, item.sortPriority });
			}
		} else {
			auto randomCount = GeneralUtils::GenerateRandomNumber<int32_t>(lootMatrix.minToDrop, lootMatrix.maxToDrop);

			for (size_t i = 0; i < randomCount; i++) {
				if (vendorItems.empty()) break;

				auto randomItemIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, vendorItems.size() - 1);

				const auto& randomItem = vendorItems[randomItemIndex];

				vendorItems.erase(vendorItems.begin() + randomItemIndex);

				m_Inventory.insert({ randomItem.itemid, randomItem.sortPriority });
			}
		}
	}

	//Because I want a vendor to sell these cameras
	if (m_Parent->GetLOT() == 13569) {
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
	m_Parent->AddCallbackTimer(m_RefreshTimeSeconds, [this]() {
		RefreshInventory();
		});
	GameMessages::SendVendorStatusUpdate(m_Parent, UNASSIGNED_SYSTEM_ADDRESS);
}

void VendorComponent::SetupConstants() {
	auto* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	int componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), COMPONENT_TYPE_VENDOR);

	auto* vendorComponentTable = CDClientManager::Instance()->GetTable<CDVendorComponentTable>("VendorComponent");
	std::vector<CDVendorComponent> vendorComps = vendorComponentTable->Query([=](CDVendorComponent entry) { return (entry.id == componentID); });
	if (vendorComps.empty()) return;
	m_BuyScalar = vendorComps[0].buyScalar;
	m_SellScalar = vendorComps[0].sellScalar;
	m_RefreshTimeSeconds = vendorComps[0].refreshTimeSeconds;
	m_LootMatrixID = vendorComps[0].LootMatrixIndex;
}
