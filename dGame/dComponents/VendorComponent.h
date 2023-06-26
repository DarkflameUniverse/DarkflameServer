#pragma once
#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "CDClientManager.h"
#include "Component.h"
#include "Entity.h"
#include "GameMessages.h"
#include "RakNetTypes.h"
#include "eReplicaComponentType.h"

/**
 * A component for vendor NPCs. A vendor sells items to the player.
 */
class VendorComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::VENDOR;

	VendorComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	void OnUse(Entity* originator) override;

	float GetBuyScalar() const {
		return m_BuyScalar;
	}

	float GetSellScalar() const {
		return m_SellScalar;
	}

	void SetBuyScalar(float value) {
		m_BuyScalar = value;
	}

	void SetSellScalar(float value) {
		m_SellScalar = value;
	}

	std::map<LOT, int>& GetInventory() {
		return m_Inventory;
	}

	void SetHasMultiCostItems(bool hasMultiCostItems) {
		if (m_HasMultiCostItems == hasMultiCostItems) return;
		m_HasMultiCostItems = hasMultiCostItems;
		m_DirtyVendor = true;
	}
	void SetHasStandardCostItems(bool hasStandardCostItems) {
		if (m_HasStandardCostItems == hasStandardCostItems) return;
		m_HasStandardCostItems = hasStandardCostItems;
		m_DirtyVendor = true;
	}

	/**
	 * Refresh the inventory of this vendor.
	 */
	void RefreshInventory(bool isCreation = false);

	/**
	 * Called on startup of vendor to setup the variables for the component.
	 */
	void SetupConstants();

	bool SellsItem(const LOT item) const {
		return m_Inventory.find(item) != m_Inventory.end();
	}
private:
	/**
	 * The buy scalar.
	 */
	float m_BuyScalar;

	/**
	 * The sell scalar.
	 */
	float m_SellScalar;

	/**
	 * The refresh time of this vendors' inventory.
	 */
	float m_RefreshTimeSeconds;

	/**
	 * Loot matrix id of this vendor.
	 */
	uint32_t m_LootMatrixID;

	/**
	 * The list of items the vendor sells.
	 */
	std::map<LOT, int> m_Inventory;

	bool m_DirtyVendor;
	bool m_HasStandardCostItems;
	bool m_HasMultiCostItems;
};

#endif // VENDORCOMPONENT_H
