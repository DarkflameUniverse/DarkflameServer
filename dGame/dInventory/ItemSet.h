#pragma once

#include <vector>

#include "dCommonVars.h"

#include "ItemSetPassiveAbility.h"

class InventoryComponent;

/**
 * A set of items that can be worn together for special bonuses, for example faction gear.
 */
class ItemSet
{
public:
	explicit ItemSet(uint32_t id, InventoryComponent* inventoryComponent);
	void Update(float deltaTime);

	/**
	 * Returns if this item set contains the LOT specified
	 * @param lot the lot to check for
	 * @return if this item set contains the LOT specified
	 */
	bool Contains(LOT lot);

	/**
	 * Equips the item set skill for this LOT (if it's in the item set)
	 * @param lot the LOT of the item to equip skills for
	 */
	void OnEquip(LOT lot);

	/**
	 * Unequips the item set skill for this LOT (if it's in the item set)
	 * @param lot the LOT of the item to unequip skills for
	 */
	void OnUnEquip(LOT lot);

	/**
	 * Returns the number of items in the item set that are currently equipped
	 * @return the number of items in the item set that are currently equipped
	 */
	uint32_t GetEquippedCount() const;

	/**
	 * Returns the ID of this item set
	 * @return the ID of this item set
	 */
	uint32_t GetID() const;

	/**
	 * Triggers all the passive abilities in this item set that match this trigger
	 * @param trigger the trigger to use to trigger passive abilities
	 */
	void TriggerPassiveAbility(PassiveAbilityTrigger trigger);

	/**
	 * Returns the skills that can be equipped for a specified amount of equipped items
	 * @param itemCount the amount of items equipped to check for
	 * @return the skills that can be equipped for a specified amount of equipped items
	 */
	std::vector<uint32_t> GetSkillSet(uint32_t itemCount) const;

private:
	/**
	 * The ID of this skill set
	 */
	uint32_t m_ID;

	/**
	 * The inventory this skill set belongs to
	 */
	InventoryComponent* m_InventoryComponent;

	/**
	 * The items in the skill set that are currently equipped
	 */
	std::vector<LOT> m_Equipped;

	/**
	 * The total list of items in this skill set
	 */
	std::vector<LOT> m_Items;

	/**
	 * The skills that can be triggered when 2 items are equipped
	 */
	std::vector<uint32_t> m_SkillsWith2;

	/**
	 * The skills that can be triggered when 3 items are equipped
	 */
	std::vector<uint32_t> m_SkillsWith3;

	/**
	 * The skills that can be triggered when 4 items are equipped
	 */
	std::vector<uint32_t> m_SkillsWith4;

	/**
	 * The skills that can be triggered when 5 items are equipped
	 */
	std::vector<uint32_t> m_SkillsWith5;

	/**
	 * The skills that can be triggered when 6 items are equipped
	 */
	std::vector<uint32_t> m_SkillsWith6;

	/**
	 * The passive abilities associated with this skill set
	 */
	std::vector<ItemSetPassiveAbility> m_PassiveAbilities;
};
