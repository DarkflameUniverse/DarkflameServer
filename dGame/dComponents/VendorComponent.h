#pragma once
#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "CDClientManager.h"
#include "Component.h"
#include "Entity.h"
#include "GameMessages.h"
#include "RakNetTypes.h"

/**
 * A component for vendor NPCs. A vendor sells items to the player.
 */
class VendorComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_VENDOR;

	VendorComponent(Entity* parent);
	~VendorComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	void OnUse(Entity* originator) override;

	/**
	 * Gets the buy scaler
	 * @return the buy scaler
	 */
	float GetBuyScalar() const;

	/**
	 * Sets the buy scalar.
	 * @param value the new value.
	 */
	void SetBuyScalar(float value);

	/**
	 * Gets the buy scaler
	 * @return the buy scaler
	 */
	float GetSellScalar() const;

	/**
	 * Sets the sell scalar.
	 * @param value the new value.
	 */
	void SetSellScalar(float value);

	/**
	 * True if the NPC LOT is 13800, the only NPC with a crafting station.
	 */
	bool HasCraftingStation();

	/**
	 * Gets the list if items the vendor sells.
	 * @return the list of items.
	 */
	std::map<LOT, int>& GetInventory();

	/**
	 * Refresh the inventory of this vendor.
	 */
	void RefreshInventory(bool isCreation = false);

	/**
	 * Called on startup of vendor to setup the variables for the component.
	 */
	void SetupConstants();
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
};

#endif // VENDORCOMPONENT_H
