#pragma once

#ifndef INVENTORYCOMPONENT_H
#define INVENTORYCOMPONENT_H

#include <map>
#include <stack>


#include "BehaviorSlot.h"
#include "tinyxml2.h"

#include "dCommonVars.h"
#include "EquippedItem.h"
#include "Inventory.h"
#include "LDFFormat.h"
#include "DatabasePet.h"
#include "Component.h"
#include "ItemSetPassiveAbility.h"
#include "ItemSetPassiveAbilityID.h"
#include "PossessorComponent.h"

class Entity;
class ItemSet;

typedef std::map<std::string, EquippedItem> EquipmentMap;

enum class eItemType : int32_t;

/**
 * Handles the inventory of entity, including the items they possess and have equipped. An entity can have inventories
 * of different types, each type representing a different group of items, see `eInventoryType` for a list of
 * inventories.
 */
class InventoryComponent : public Component
{
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_INVENTORY;
	explicit InventoryComponent(Entity* parent, tinyxml2::XMLDocument* document = nullptr);

    void Update(float deltaTime) override;
    void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
    void LoadXml(tinyxml2::XMLDocument* document);
    void UpdateXml(tinyxml2::XMLDocument* document) override;
    void ResetFlags();

    /**
     * Returns an inventory of the specified type, if it exists
     * @param type the inventory type to find an inventory for
     * @return the inventory of the specified type
     */
	Inventory* GetInventory(eInventoryType type);

    /**
     * Returns all the inventories this entity has, indexed by type
     * @return all the inventories this entity has, indexed by type
     */
	const std::map<eInventoryType, Inventory*>& GetInventories() const;

    /**
     * Returns the amount of items this entity possesses of a certain LOT
     * @param lot the lot to search for
     * @return the amount of items this entity possesses the specified LOT
     */
	uint32_t GetLotCount(LOT lot) const;

    /**
     * Returns the amount of items this entity possesses of a LOT, given that they're not in a temporary inventory
     * (vendor buyback, vault, etc).
     * @param lot the lot to search for
     * @return the amount of items this entity possesses of the specified lot
     */
	uint32_t GetLotCountNonTransfer(LOT lot) const;

    /**
     * Returns the items that are currently equipped by this entity
     * @return the items that are currently equipped by this entity
     */
	const EquipmentMap& GetEquippedItems() const;

    /**
     * Adds an item to the inventory of the entity
     * @param lot the lot to add
     * @param count the amount of items to add
     * @param inventoryType the inventory to add the item to
     * @param config optional config for this item, used for example for rockets
     * @param parent optional parent of this item, used for proxy items
     * @param showFlyingLoot show a client animation if the item is added
     * @param isModMoveAndEquip equips the item
     * @param subKey optional sub ID of a related object, used by pets
     * @param inventorySourceType if the inventory was moved, the source inventory
     * @param sourceType the source of the item, used to determine if the item is dropped or mailed if the inventory is full
     * @param bound whether this item is bound
     * @param preferredSlot the preferred slot to store this item
     * @param lootSourceType The source of the loot.  Defaults to none.
     */
	void AddItem(
		LOT lot,
		uint32_t count,
        eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE,
		eInventoryType inventoryType = INVALID,
		const std::vector<LDFBaseData*>& config = {},
		LWOOBJID parent = LWOOBJID_EMPTY,
		bool showFlyingLoot = true,
		bool isModMoveAndEquip = false,
		LWOOBJID subKey = LWOOBJID_EMPTY,
		eInventoryType inventorySourceType = INVALID,
		int32_t sourceType = 0,
		bool bound = false,
		int32_t preferredSlot = -1
	);

    /**
     * Removes a LOT from the inventory
     * @param lot the lot to remove
     * @param count the number of items to remove
     * @param inventoryType optional inventory type to remove the item from
     * @param ignoreBound ignores bound items
     */
	void RemoveItem(LOT lot, uint32_t count, eInventoryType inventoryType = INVALID, bool ignoreBound = false);

    /**
     * Moves an existing item to an inventory of the entity
     * @param item the item to add
     * @param inventory the inventory to add the item to
     * @param count the number of items to add
     * @param showFlyingLot displays UI animation to the user
     * @param isModMoveAndEquip equips the item
     * @param ignoreEquipped does not stack on equipped items
     * @param preferredSlot the preferred slot to store the item in
     */
	void MoveItemToInventory(Item* item, eInventoryType inventory, uint32_t count, bool showFlyingLot = true, bool isModMoveAndEquip = false, bool ignoreEquipped = false, int32_t preferredSlot = -1);

    /**
     * Moves a stack of items to an inventory
     * @param item the item to move
     * @param inventory the inventory to move the item to
     * @param slot the slot in the inventory to move the item to
     */
	void MoveStack(Item* item, eInventoryType inventory, uint32_t slot = 0);

    /**
     * Returns an item in the inventory by object ID
     * @param id the id of the item to find
     * @return item in the inventory by object ID
     */
	Item* FindItemById(LWOOBJID id) const;

    /**
     * Returns an item in the inventory that matches the specified LOT
     * @param lot the lot of the item to find
     * @param inventoryType optional inventory to search in
     * @param ignoreEquipped ignores items that are equipped
     * @param ignoreBound ignores items that are bound
     * @return item in the inventory that matches the specified LOT
     */
	Item* FindItemByLot(LOT lot, eInventoryType inventoryType = INVALID, bool ignoreEquipped = false, bool ignoreBound = false);

    /**
     * Finds an item in the inventory that has the specified subkey, useful for pets
     * @param id the subkey to look for
     * @param inventoryType optional inventory type to search in
     * @return item in the inventory that has the specified subkey
     */
	Item* FindItemBySubKey(LWOOBJID id, eInventoryType inventoryType = INVALID);

    /**
     * Checks if the entity has enough space for a batch of loot
     * @param loot a map of items to add and how many to add
     * @return whether the entity has enough space for all the items
     */
	bool HasSpaceForLoot(const std::unordered_map<LOT, int32_t>& loot);

	/**
	 * Equips an item in the specified slot
	 * @param location the location to store the item (e.g. chest, left hand, etc.)
	 * @param item the item to place
	 * @param keepCurrent stores the item in an additional temp slot if there's already an item equipped
	 */
	void UpdateSlot(const std::string& location, EquippedItem item, bool keepCurrent = false);

    /**
     * Removes a slot from the inventory
     * @param location the slot to remove
     */
	void RemoveSlot(const std::string& location);

    /**
     * Equips the given item, guesses the slot to equip it in
     * @param item the item to equip
     * @param skipChecks skips checks for equipping cars and rockets (e.g. no special behavior follows)
     */
	void EquipItem(Item* item, bool skipChecks = false);

    /**
     * Unequips an item from the inventory
     * @param item the item to unequip
     */
	void UnEquipItem(Item* item);

    /**
     * Adds a buff related to equipping a lot to the entity
     * @param item the item to find buffs for
     */
	void ApplyBuff(Item* item) const;

    /**
     * Removes buffs related to equipping a lot from the entity
     * @param item the item to find buffs for
     */
	void RemoveBuff(Item* item) const;

    /**
     * Saves the equipped items into a temp state
     */
	void PushEquippedItems();

    /**
     * Unequips all the temporary items and equips the previous item state
     */
	void PopEquippedItems();

    /**
     * Returns if the entity has an item equipped of the given lot
     * @param lot to lot to search for
     * @return if the entity has an item equipped of the given lot
     */
	bool IsEquipped(LOT lot) const;

    /**
     * Checks and ensures that we have loaded the item set that might be related to this item
     * @param lot the lot to check the item set for
     */
	void CheckItemSet(LOT lot);

    /**
     * Sets the current consumable lot
     * @param lot the lot to set as consumable
     */
	void SetConsumable(LOT lot);

    /**
     * Returns the current consumable lot
     * @return the current consumable lot
     */
	LOT GetConsumable() const;

    /**
     * Finds all the buffs related to a lot
     * @param item the item to get the buffs for
     * @param castOnEquip if true, the skill missions for these buffs will be progressed
     * @return the buffs related to the specified lot
     */
	std::vector<uint32_t> FindBuffs(Item* item, bool castOnEquip) const;

    /**
     * Initializes the equipped items with a list of items
     * @param items the items to equip
     */
	void SetNPCItems(const std::vector<LOT>& items);

	/**
	 * Adds a skill related to a passed item to the currently equipped skills
	 * @param lot the lot to add a skill for
	 */
	void AddItemSkills(LOT lot);

    /**
     * Removes the skills related to the passed LOT from the currently equipped skills
     * @param lot the lot to remove
     */
	void RemoveItemSkills(LOT lot);

    /**
     * Triggers one of the passive abilities from the equipped item set
     * @param trigger the trigger to fire
     */
	void TriggerPassiveAbility(PassiveAbilityTrigger trigger);

    /**
     * Returns if the entity has any of the passed passive abilities equipped
     * @param passiveIDs the IDs to check for
     * @param equipmentRequirement the number of equipment required to be allowed to have the ability
     * @return if the entity has any of the passed passive abilities equipped
     */
	bool HasAnyPassive(const std::vector<ItemSetPassiveAbilityID>& passiveIDs, int32_t equipmentRequirement) const;

    /**
     * Despawns the currently active pet, if any
     */
	void DespawnPet();

    /**
     * Spawns the item as a pet (if it is one)
     * @param item the pet to spawn
     */
	void SpawnPet(Item* item);

    /**
     * Updates the database pet data for an item (e.g. moderation status)
     * @param id the id of the pet to find
     * @param data the data to store on the pet
     */
	void SetDatabasePet(LWOOBJID id, const DatabasePet& data);

    /**
     * Returns the database pet information for an object
     * @param id the object ID to search for
     * @return the database pet information for the object that belongs to the passed id
     */
	const DatabasePet& GetDatabasePet(LWOOBJID id) const;

    /**
     * Checks if the provided object ID is in this inventory and is a pet
     * @param id the id of the object to check for
     * @return if the provided object ID is in this inventory and is a pet
     */
	bool IsPet(LWOOBJID id) const;

    /**
     * Removes pet database information from the item with the specified object id
     * @param id the object id to remove pet info for
     */
	void RemoveDatabasePet(LWOOBJID id);

    /**
     * Returns the current behavior slot active for the passed item type
     * @param type the item type to find the behavior slot for
     * @return the current behavior slot active for the passed item type
     */
	static BehaviorSlot FindBehaviorSlot(eItemType type);

    /**
     * Checks if the inventory type is a temp inventory
     * @param type the inventory type to check
     * @return if the inventory type is a temp inventory
     */
	static bool IsTransferInventory(eInventoryType type);

    /**
     * Finds the skill related to the passed LOT from the ObjectSkills table
     * @param lot the lot to find
     * @return the skill related to the passed LOT
     */
	static uint32_t FindSkill(LOT lot);
	
	~InventoryComponent() override;
	
private:
    /**
     * All the inventory this entity possesses
     */
	std::map<eInventoryType, Inventory*> m_Inventories;

    /**
     * The skills that this entity currently has active
     */
	std::map<BehaviorSlot, uint32_t> m_Skills;

    /**
     * The pets this entity has, mapped by object ID and pet info
     */
	std::unordered_map<LWOOBJID, DatabasePet> m_Pets;

    /**
     * Cache of item sets this entity has encountered
     */
	std::vector<ItemSet*> m_Itemsets;

    /**
     * The LOTs we've checked all the item sets for (for cache reasons)
     */
	std::vector<LOT> m_ItemSetsChecked;

    /**
     * all the equipped items
     */
	EquipmentMap m_Equipped;

    /**
     * Clone of the equipped items before unequipping all of them
     */
	EquipmentMap m_Pushed;

    /**
     * If the inventory has changed
     */
	bool m_Dirty;

    /**
     * The currently active consumable
     */
	LOT m_Consumable;

    /**
     * Currently has a car equipped
     */
    bool hasCarEquipped = false;
    Entity* equippedCarEntity = nullptr;
    LWOOBJID previousPossessableID = LWOOBJID_EMPTY;
    LWOOBJID previousPossessorID = LWOOBJID_EMPTY;
    /**
     * Creates all the proxy items (subitems) for a parent item
     * @param parent the parent item to generate all the subitems for
     * @return the proxy items (subitems) for a parent item
     */
	std::vector<Item*> GenerateProxies(Item* parent);

    /**
     * Finds all the proxy items in this inventory for a given parent item
     * @param parent the parent to find proxy items for
     * @return the proxy items for the parent
     */
	std::vector<Item*> FindProxies(LWOOBJID parent);

    /**
     * Returns true if the provided LWOOBJID is the parent of this Item.
     * @param parent the parent item to check for proxies
     * @return if the provided ID is a valid proxy item
     */
	bool IsValidProxy(LWOOBJID parent);

    /**
     * Returns if the provided ID is a valid proxy item (e.g. we have children for it)
     * @param parent the parent item to check for
     * @return if the provided ID is a valid proxy item
     */
	bool IsParentValid(Item* root);

    /**
     * Removes all the proxy items that have a dangling parent
     */
	void CheckProxyIntegrity();

    /**
     * Removes all the proxy items for a given parent from the inventory
     * @param item the item to remove proxy items for
     */
	void PurgeProxies(Item* item);

    /**
     * Saves all the pet information stored in inventory items to the database
     * @param document the xml doc to save to
     */
	void LoadPetXml(tinyxml2::XMLDocument* document);

    /**
     * Loads all the pet information from an xml doc into items
     * @param document the xml doc to load from
     */
	void UpdatePetXml(tinyxml2::XMLDocument* document);
};

#endif
