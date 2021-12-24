#include "VendorComponent.h"
#include "Game.h"
#include "dServer.h"

#include <BitStream.h>

VendorComponent::VendorComponent(Entity* parent) : Component(parent) {
    auto* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
    auto* vendorComponentTable = CDClientManager::Instance()->GetTable<CDVendorComponentTable>("VendorComponent");
    auto* lootMatrixTable = CDClientManager::Instance()->GetTable<CDLootMatrixTable>("LootMatrix");
    auto* lootTableTable = CDClientManager::Instance()->GetTable<CDLootTableTable>("LootTable");

    int componentID = compRegistryTable->GetByIDAndType(m_Parent->GetLOT(), COMPONENT_TYPE_VENDOR);
    std::vector<CDVendorComponent> vendorComps = vendorComponentTable->Query([=](CDVendorComponent entry) { return (entry.id == componentID); });
    if (vendorComps.empty()) {
        return;
    }
    m_BuyScalar = vendorComps[0].buyScalar;
    m_SellScalar = vendorComps[0].sellScalar;
    int lootMatrixID = vendorComps[0].LootMatrixIndex;
    std::vector<CDLootMatrix> lootMatrices = lootMatrixTable->Query([=](CDLootMatrix entry) { return (entry.LootMatrixIndex == lootMatrixID); });
    if (lootMatrices.empty()) {
        return;
    }
    for (const auto& lootMatrix : lootMatrices) {
        int lootTableID = lootMatrix.LootTableIndex;
        std::vector<CDLootTable> vendorItems = lootTableTable->Query([=](CDLootTable entry) { return (entry.LootTableIndex == lootTableID); });
        if (lootMatrix.maxToDrop == 0 || lootMatrix.minToDrop == 0) {
            for (CDLootTable item : vendorItems) {
                m_Inventory.insert({item.itemid, item.sortPriority});
            }
        } else {
            auto randomCount = GeneralUtils::GenerateRandomNumber<int32_t>(lootMatrix.minToDrop, lootMatrix.maxToDrop);

            for (size_t i = 0; i < randomCount; i++) {
                if (vendorItems.empty()) {
                    break;
                }

                auto randomItemIndex = GeneralUtils::GenerateRandomNumber<int32_t>(0, vendorItems.size() - 1);

                const auto& randomItem = vendorItems[randomItemIndex];

                vendorItems.erase(vendorItems.begin() + randomItemIndex);

                m_Inventory.insert({randomItem.itemid, randomItem.sortPriority});
            }
        }
    }

	//Because I want a vendor to sell these cameras
    if (parent->GetLOT() == 13569) {
        auto randomCamera = GeneralUtils::GenerateRandomNumber<int32_t>(0, 2);

        switch (randomCamera) {
            case 0:
                m_Inventory.insert({16253, 0}); //Grungagroid
                break;
            case 1:
                m_Inventory.insert({16254, 0}); //Hipstabrick
                break;
            case 2:
                m_Inventory.insert({16204, 0}); //Megabrixel snapshot
                break;
            default:
                break;
        }
    }

	//Custom code for Max vanity NPC
	if (parent->GetLOT() == 9749 && Game::server->GetZoneID() == 1201) {
		m_Inventory.clear();
		m_Inventory.insert({11909, 0}); //Top hat w frog
		m_Inventory.insert({7785, 0}); //Flash bulb
		m_Inventory.insert({12764, 0}); //Big fountain soda
		m_Inventory.insert({12241, 0}); //Hot cocoa (from fb)
	}

    if (parent->GetLOT() == 9749 && Game::server->GetZoneID() == 1200) {
		m_Inventory.clear();
		m_Inventory.insert({12237, 0});
		m_Inventory.insert({12338, 0});
		m_Inventory.insert({12299, 0});
		m_Inventory.insert({12291, 0});
		m_Inventory.insert({12296, 0});
		m_Inventory.insert({12242, 0});
        m_Inventory.insert({12241, 0});
	}
}

VendorComponent::~VendorComponent() = default;

void VendorComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
    outBitStream->Write1();
    outBitStream->Write1(); // this bit is REQUIRED for vendor + mission multiinteract
    outBitStream->Write(HasCraftingStation());
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
