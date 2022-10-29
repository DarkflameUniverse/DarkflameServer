#pragma once

#ifndef INVENTORY_H
#define INVENTORY_H

#include <map>
#include <vector>


#include "CDClientManager.h"
#include "dCommonVars.h"

class Item;
class InventoryComponent;

/**
 * An inventory of a certain type that's owned by an entity
 */
class Inventory final
{
public:
	explicit Inventory(eInventoryType type, uint32_t size, const std::vector<Item*>& items, InventoryComponent* component);

	/**
	 * Returns the type of this inventory
	 * @return the type of this inventory
	 */
	eInventoryType GetType() const;

	/**
	 * Returns the maximum amount of items this inventory can contain
	 * @return the maximum amount of items this inventory can contain
	 */
	uint32_t GetSize() const;

	/**
	 * Returns all the items that are currently in this inventory, mapped by object ID
	 * @return all the items that are currently in this inventory, mapped by object ID
	 */
	std::map<LWOOBJID, Item*>& GetItems();

	/**
	 * Returns all the items that are currently in this inventory, mapped by slot
	 * @return all the items that are currently in this inventory, mapped by slot
	 */
	std::map<uint32_t, Item*> GetSlots() const;

	/**
	 * Returns the inventory component that this inventory is part of
	 * @return the inventory component that this inventory is part of
	 */
	InventoryComponent* GetComponent() const;

	/**
	 * Returns the amount of items this inventory contains of the specified LOT
	 * @param lot the lot to find items for
	 * @return the amount of items this inventory contains of the specified LOT
	 */
	uint32_t GetLotCount(LOT lot) const;

	/**
	 * Updates the max size of this inventory
	 * @param value the size to set
	 */
	void SetSize(uint32_t value);

	/**
	 * Returns the first slot in this inventory that does not contain an item
	 * @return the first slot in this inventory that does not contain an item
	 */
	int32_t FindEmptySlot();

	/**
	 * Returns the number of empty slots this inventory has left
	 * @return the number of empty slots this inventory has left
	 */
	int32_t GetEmptySlots();

	/**
	 * Returns if the slot for the specified index is empty
	 * @param slot the index to check occupation for
	 * @return if the slot for the specified index is empty
	 */
	bool IsSlotEmpty(int32_t slot);

	/**
	 * Finds an item in this inventory by the provided id
	 * @param id the object ID of the item to find
	 * @return item in this inventory by the provided id
	 */
	Item* FindItemById(LWOOBJID id) const;

	/**
	 * Finds an item in the inventory for the provided LOT
	 * @param lot the lot to find items for
	 * @param ignoreEquipped ignores equipped items
	 * @param ignoreBound ignores bound items
	 * @return item in the inventory for the provided LOT
	 */
	Item* FindItemByLot(LOT lot, bool ignoreEquipped = false, bool ignoreBound = false) const;

	/**
	 * Finds an item in the inventory stored on the provied slot
	 * @param slot to slot to find an item for
	 * @return item in the inventory stored on the provied slot
	 */
	Item* FindItemBySlot(uint32_t slot) const;

	/**
	 * Finds an item based on a specified subkey (useful for pets)
	 * @param id the subkey to look for in the items
	 * @return item based on a specified subkey
	 */
	Item* FindItemBySubKey(LWOOBJID id) const;

	/**
	 * Adds an item to the inventory, finding a slot to place it in
	 * @param item item to add to the inventory
	 */
	void AddManagedItem(Item* item);

	/**
	 * Removes an item from the inventory, clearing its slot
	 * @param item
	 */
	void RemoveManagedItem(Item* item);

	/**
	 * Returns the inventory type an item of the specified lot should be placed in
	 * @param lot the lot to find the inventory type for
	 * @return the inventory type an item of the specified lot should be placed in
	 */
	static eInventoryType FindInventoryTypeForLot(LOT lot);

	/**
	 * Finds the database item component for a item of a certain LOT
	 * @param lot the LOT of the item to get the database item component for
	 * @return the database item component for a item of a certain LOT
	 */
	static const CDItemComponent& FindItemComponent(LOT lot);

	/**
	 * Cheks if the provided lot has a database item component
	 * @param lot the LOT to check item validity for
	 * @return if the provided lot has a database item component
	 */
	static bool IsValidItem(LOT lot);

	/**
	 * Returns all the items that are restricted to GMs
	 * @return all the items that are restricted to GMs
	 */
	static const std::vector<LOT>& GetAllGMItems();

	~Inventory();

private:
	/**
	 * The type of this inventory
	 */
	eInventoryType type;

	/**
	 * The max size of this inventory
	 */
	uint32_t size;

	/**
	 * The amount of items that can still be stored in this inventroy
	 */
	uint32_t free;

	/**
	 * The items stored in this inventory
	 */
	std::map<LWOOBJID, Item*> items;

	/**
	 * The inventory component this inventory belongs to
	 */
	InventoryComponent* component;

	/**
	 * List of items that are GM restricted
	 */
	static std::vector<LOT> m_GameMasterRestrictedItems;
};

#endif
