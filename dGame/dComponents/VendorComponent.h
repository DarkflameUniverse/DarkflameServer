#ifndef VENDORCOMPONENT_H
#define VENDORCOMPONENT_H

#include "RakNetTypes.h"
#include "Entity.h"
#include "GameMessages.h"
#include "CDClientManager.h"
#include "Component.h"

/**
 * A component for vendor NPCs. A vendor sells items to the player.
 */
class VendorComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_VENDOR;
	
	VendorComponent(Entity* parent);
	~VendorComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	
	void OnUse(Entity* originator);
	
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

private:
	/**
	 * The buy scaler.
	 */
	float m_BuyScalar;

	/**
	 * The sell scaler.
	 */
	float m_SellScalar;

	/**
	 * The list of items the vendor sells.
	 */
	std::map<LOT, int> m_Inventory;
};

#endif // VENDORCOMPONENT_H
