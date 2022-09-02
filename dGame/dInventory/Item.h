#pragma once

#include "dCommonVars.h"
#include "Inventory.h"
#include "LDFFormat.h"
#include "CDClientManager.h"
#include "dLogger.h"
#include "Preconditions.h"

/**
 * An item that can be stored in an inventory and optionally consumed or equipped
 * TODO: ideally this should be a component
 */
class Item final
{
public:

	/**
	 * Creates an item, should be used if the item is not picked up but already exists
	 * @param id the object ID of the item to create
	 * @param lot the LOT of the item
	 * @param inventory the inventory to add this item to
	 * @param slot the slot in the inventory to add this item to
	 * @param count the amount of items to add to the inventory
	 * @param bound if the item should be bound
	 * @param config config data for this item, e.g. for rockets
	 * @param parent optional parent of this item, e.g. for proxy items
	 * @param subKey optional subkey for this item, e.g. for pets
	 */
	explicit Item(
		LWOOBJID id,
		LOT lot,
		Inventory* inventory,
		uint32_t slot,
		uint32_t count,
		bool bound,
		const std::vector<LDFBaseData*>& config,
		LWOOBJID parent,
		LWOOBJID subKey,
		eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE
	);

	/**
	 * Creates an item, should be used if the item is picked up / added to the inventory after load
	 * @param lot the LOT of the item
	 * @param inventory the inventory to add this item to
	 * @param slot the slot in the inventory to add this item to
	 * @param count the amount of items to add to the inventory
	 * @param config config data for this item, e.g. for rockets
	 * @param parent optional parent of this item, e.g. for proxy items
	 * @param showFlyingLoot show UI animation of the item being added
	 * @param isModMoveAndEquip equips the item
	 * @param subKey optional subkey for this item, e.g. for pets
	 * @param bound if the item should be bound
	 */
	explicit Item(
		LOT lot,
		Inventory* inventory,
		uint32_t slot = 0,
		uint32_t count = 1,
		const std::vector<LDFBaseData*>& config = {},
		LWOOBJID parent = LWOOBJID_EMPTY,
		bool showFlyingLoot = true,
		bool isModMoveAndEquip = false,
		LWOOBJID subKey = LWOOBJID_EMPTY,
		bool bound = false,
		eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE
	);

	~Item();

	/**
	 * Returns the object ID of this item
	 * @return the object ID of this item
	 */
	LWOOBJID GetId() const;

	/**
	 * Returns the lot of this item
	 * @return the lot of this item
	 */
	LOT GetLot() const;

	/**
	 * Sets the number of items this item represents
	 * @param value the number to update by
	 * @param silent if true, the client will not be notified of the change with GMs
	 * @param disassemble if items were removed, this returns all the sub parts of the item individually if it had assembly part lots
	 * @param showFlyingLoot shows flying loot to the client, if not silent
	 */
	void SetCount(uint32_t value, bool silent = false, bool disassemble = true, bool showFlyingLoot = true, eLootSourceType lootSourceType = eLootSourceType::LOOT_SOURCE_NONE);

	/**
	 * Returns the number of items this item represents (e.g. for stacks)
	 * @return the number of items this item represents
	 */
	uint32_t GetCount() const;

	/**
	 * Sets the slot this item is stored in
	 * @param value the slot this item is stored in
	 */
	void SetSlot(uint32_t value);

	/**
	 * Returns the slot this item is in
	 * @return the slot this item is in
	 */
	uint32_t GetSlot() const;

	/**
	 * Returns current config info for this item, e.g. for rockets
	 * @return current config info for this item
	 */
	std::vector<LDFBaseData*>& GetConfig();

	/**
	 * Returns the database info for this item
	 * @return the database info for this item
	 */
	const CDItemComponent& GetInfo() const;

	/**
	 * Sets if the item is bound
	 * @param value if the item is bound
	 */
	void SetBound(bool value);

	/**
	 * Returns if the item is bound
	 * @return if the item is bound
	 */
	bool GetBound() const;

	/**
	 * Sets the inventory this item belongs to
	 * @param value the inventory this item belongs to
	 */
	void SetInventory(Inventory* value);

	/**
	 * Returns the inventory this item belongs to
	 * @return the inventory this item belongs to
	 */
	Inventory* GetInventory() const;

	/**
	 * Returns the parent of this item, e.g. for proxy items
	 * @return the parent of this item
	 */
	LWOOBJID GetParent() const;

	/**
	 * Sets the subkey for this item, e.g. for pets
	 * @param value the subkey for this item
	 */
	void SetSubKey(LWOOBJID value);

	/**
	 * Returns the sub key this item has, e.g. for pets
	 * @return the sub key this item has
	 */
	LWOOBJID GetSubKey() const;

	/**
	 * Returns the preconditions that must be met before this item may be used
	 * @return the preconditions that must be met before this item may be used
	 */
	PreconditionExpression* GetPreconditionExpression() const;

	/**
	 * Equips this item into the linked inventory
	 * @param skipChecks skips equip checks for special items like rockets and cars
	 */
	void Equip(bool skipChecks = false);

	/**
	 * Unequps the item from the linked inventory
	 */
	void UnEquip();

	/**
	 * Returns if the item is equipped in the linked inventory
	 * @return if the item is equipped
	 */
	bool IsEquipped() const;

	/**
	 * Attempts to consume one of this item, applying its skills
	 * @return whether the consumption was successful, e.g. the skill was cast
	 */
	bool Consume();

	/**
	 * Uses this item if its non equip, essentially an interface for the linked GM
	 */
	void UseNonEquip();

	/**
	 * Disassembles the part LOTs of this item back into the inventory, if it has any
	 * @param inventoryType the inventory to dissassemble into
	 */
	void Disassemble(eInventoryType inventoryType = INVALID);

	/**
	 * Disassembles this item into bricks
	 */
	void DisassembleModel();

	/**
	 * Removes the item from the linked inventory
	 */
	void RemoveFromInventory();

private:
	/**
	 * The object ID of this item
	 */
	LWOOBJID id;

	/**
	 * The LOT of this item
	 */
	LOT lot;

	/**
	 * The number of items this represents
	 */
	uint32_t count;

	/**
	 * The slot this item is stored in
	 */
	uint32_t slot;

	/**
	 * If this item is bound
	 */
	bool bound;

	/**
	 * A potential parent of this item, if this item is a subitem
	 */
	LWOOBJID parent;

	/**
	 * A potential subkey of this item, e.g. for pets
	 */
	LWOOBJID subKey;

	/**
	 * Config data for this item, e.g. for rocket parts and car parts
	 */
	std::vector<LDFBaseData*> config;

	/**
	 * The inventory this item belongs to
	 */
	Inventory* inventory;

	/**
	 * The database information of this item
	 */
	const CDItemComponent* info;

	/**
	 * A precondition to using this item
	 */
	PreconditionExpression* preconditions = nullptr;
};
