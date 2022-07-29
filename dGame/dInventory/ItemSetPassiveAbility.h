#pragma once

#include <vector>

#include "dCommonVars.h"

class Entity;
class ItemSet;

enum class PassiveAbilityTrigger
{
	AssemblyImagination, // Less than 1 imagination
	ParadoxHealth, // Less or equal to 1 health
	SentinelArmor, // Less than 1 armor
	VentureHealth, // Less than 3 health
	EnemySmashed, // Enemy is smashed
};

/**
 * Passive abilities that belong to an item set, activated when a PassiveAbilityTrigger condition is met
 */
class ItemSetPassiveAbility
{
public:
	ItemSetPassiveAbility(PassiveAbilityTrigger trigger, Entity* parent, ItemSet* itemSet);
	~ItemSetPassiveAbility();
	void Update(float deltaTime);

	/**
	 * Attempts to trigger a passive ability for this item set, if this is the wrong trigger this is a no-op
	 * @param trigger the trigger to attempt to fire
	 */
	void Trigger(PassiveAbilityTrigger trigger);

	/**
	 * Activates the passive ability
	 */
	void Activate();

	/**
	 * Finds all the passive abilities associated with a certain item set
	 * @param itemSetID the item set to find abilities for
	 * @param parent the parent to add to the passive abilities
	 * @param itemSet the item set to add to the passive abilities
	 * @return the passive abilities for the provided item set
	 */
	static std::vector<ItemSetPassiveAbility> FindAbilities(uint32_t itemSetID, Entity* parent, ItemSet* itemSet);

private:
	void OnEnemySmshed();

	/**
	 * The means of triggering this ability
	 */
	PassiveAbilityTrigger m_Trigger;

	/**
	 * The owner of this ability
	 */
	Entity* m_Parent;

	/**
	 * The item set this ability belongs to
	 */
	ItemSet* m_ItemSet;

	/**
	 * The cooldown on this ability until it can be activated again
	 */
	float m_Cooldown;
};
