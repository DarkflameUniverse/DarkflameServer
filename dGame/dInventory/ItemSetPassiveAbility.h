#pragma once

#include <array>
#include <string_view>
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
 * What a stat-triggered set ability does, as set up by one of the live
 * server's res/scripts/equipmenttriggers item scripts.
 */
struct ItemSetStatTrigger {
	PassiveAbilityTrigger trigger; // the Lua "trigger" var (stat + threshold)
	uint32_t skillID; // the Lua "skillID" var
	uint32_t itemsRequired; // the Lua "itemsRequired" var
	uint32_t setID; // the Lua "skillSet" var: the ItemSets row the script belongs to
	float cooldown; // the Lua "factionCooldownTime" var; 0 for scripts without "isFactionSkill"
};

/**
 * One set skill that runs when the wearer smashes something: a skill in the set's
 * ItemSetSkills whose root behavior is DarkInspiration (the client's "OnKillFactionBehavior").
 */
struct ItemSetOnKillSkill {
	uint32_t itemsRequired; // the skillSetWithN tier the skill comes from
	uint32_t skillID;
	uint32_t behaviorID;
};

/**
 * Passive abilities that belong to an item set, activated when a PassiveAbilityTrigger condition is met
 */
class ItemSetPassiveAbility
{
public:
	ItemSetPassiveAbility(PassiveAbilityTrigger trigger, Entity* parent, ItemSet* itemSet);
	void Update(float deltaTime);

	/**
	 * Attempts to trigger a passive ability for this item set, if this is the wrong trigger this is a no-op
	 * @param trigger the trigger to attempt to fire
	 */
	void Trigger(PassiveAbilityTrigger trigger, Entity* target = nullptr);

	/**
	 * Activates the passive ability
	 */
	void Activate(Entity* target = nullptr);

	/**
	 * Finds all the passive abilities of an item set, from the CDClient and the item scripts on its items
	 * @param itemSetID the item set to find abilities for
	 * @param items the LOTs in the set (ItemSets.itemIDs)
	 * @param skillSets the skills of each skillSetWithN tier, index 0 being skillSetWith2
	 * @param parent the parent to add to the passive abilities
	 * @param itemSet the item set to add to the passive abilities
	 * @return the passive abilities for the provided item set
	 */
	static std::vector<ItemSetPassiveAbility> FindAbilities(uint32_t itemSetID, const std::vector<LOT>& items, const std::array<std::vector<uint32_t>, 5>& skillSets, Entity* parent, ItemSet* itemSet);

	/**
	 * The stat triggers the set's items carry. The live server set these up in the equipmenttriggers
	 * item scripts, which are linked to the items through their ScriptComponent.
	 */
	static std::vector<ItemSetStatTrigger> FindStatTriggers(uint32_t itemSetID, const std::vector<LOT>& items);

	/**
	 * The on-kill skills among the set's skills: those whose root behavior is DarkInspiration.
	 */
	static std::vector<ItemSetOnKillSkill> FindOnKillSkills(const std::array<std::vector<uint32_t>, 5>& skillSets);

	/**
	 * The trigger a live equipmenttriggers script sets up, by the script's file name (any path, any case).
	 * @return nullptr when the script is not a set stat trigger
	 */
	static const ItemSetStatTrigger* GetStatTriggerForScript(std::string_view scriptName);

private:
	void OnEnemySmashed(Entity* target);

	/**
	 * The means of triggering this ability
	 */
	PassiveAbilityTrigger m_Trigger;

	/**
	 * What a stat trigger does, unused for EnemySmashed
	 */
	ItemSetStatTrigger m_StatTrigger{};

	/**
	 * The skills an EnemySmashed ability runs
	 */
	std::vector<ItemSetOnKillSkill> m_OnKillSkills;

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
