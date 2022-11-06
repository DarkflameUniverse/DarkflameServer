#ifndef DESTROYABLECOMPONENT_H
#define DESTROYABLECOMPONENT_H

#include "RakNetTypes.h"
#include <vector>
#include "tinyxml2.h"
#include "Entity.h"
#include "Component.h"

/**
 * Represents the stats of an entity, for example its health, imagination and armor. Also handles factions, which
 * indicate which enemies this entity has.
 */
class DestroyableComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_DESTROYABLE;

	DestroyableComponent(Entity* parentEntity);
	~DestroyableComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, uint32_t& flags);
	void LoadFromXml(tinyxml2::XMLDocument* doc) override;
	void UpdateXml(tinyxml2::XMLDocument* doc) override;

	/**
	 * Initializes the component using a different LOT
	 * @param templateID the ID to use for initialization
	 */
	void Reinitialize(LOT templateID);

	/**
	 * Sets the health of this entity. Makes sure this is serialized on the next tick and if this is a character its
	 * stats will also update.
	 * @param value the new health value
	 */
	void SetHealth(int32_t value);

	/**
	 * Heals the entity by some delta amount
	 * @param health the delta amount to heal
	 */
	void Heal(uint32_t health);

	/**
	 * Returns the current health of this entity
	 * @return the current health of this entity
	 */
	int32_t GetHealth() const { return m_iHealth; }

	/**
	 * Updates the max health this entity has (e.g. what it can heal to), and optionally displays a UI animation indicating that
	 * @param value the max health value to set
	 * @param playAnim whether or not to play a UI animation indicating the change in max health
	 */
	void SetMaxHealth(float value, bool playAnim = false);

	/**
	 * Returns the curent max health of this entity
	 * @return the current max health of this entity
	 */
	float GetMaxHealth() const { return m_fMaxHealth; }

	/**
	 * Sets the armor for this entity. This also makes sure this change is serialized and if this is a character it also
	 * updates their stats.
	 * @param value the armor value to set
	 */
	void SetArmor(int32_t value);

	/**
	 * Repairs armor of this entity, updating it by a delta amount
	 * @param armor the amount of armor to repair
	 */
	void Repair(uint32_t armor);

	/**
	 * Returns the current armor value for the entity
	 * @return the current armor value for the entity
	 */
	int32_t GetArmor() const { return m_iArmor; }

	/**
	 * Updates the max armor this entity has (e.g. what it can heal to), and optionally displays a UI animation indicating that
	 * @param value the max armor value to set
	 * @param playAnim whether or not to play a UI animation indicating the change in max armor
	 */
	void SetMaxArmor(float value, bool playAnim = false);

	/**
	 * Returns the current maximum armor this entity can have
	 * @return the current maximum armor this entity can have
	 */
	float GetMaxArmor() const { return m_fMaxArmor; }

	/**
	 * Sets the imagination value for this entity. Ensures that the change is serialized and if this is a character
	 * their stats will be updated. Can also trigger the assembly passive ability to restore on 0 imag.
	 * @param value
	 */
	void SetImagination(int32_t value);

	/**
	 * Updates the imagination of this entity by a delta amount
	 * @param deltaImagination the imagination to update
	 */
	void Imagine(int32_t deltaImagination);

	/**
	 * Returns the current imagination value of this entity
	 * @return the current imagination value of this entity
	 */
	int32_t GetImagination() const { return m_iImagination; }

	/**
	 * Updates the max imagination this entity has (e.g. what it can heal to), and optionally displays a UI animation indicating that
	 * @param value the max imagination value to set
	 * @param playAnim whether or not to play a UI animation indicating the change in max imagination
	 */
	void SetMaxImagination(float value, bool playAnim = false);

	/**
	 * Returns the current max imagination value
	 * @return the current max imagination value
	 */
	float GetMaxImagination() const { return m_fMaxImagination; }

	/**
	 * Sets the damage this entity can absorb before getting hurt, also serializes this change.
	 * @param value the damage to absorb
	 */
	void SetDamageToAbsorb(int32_t value);

	/**
	 * Returns the current damage to absorb
	 * @return the current damage to absorb
	 */
	int32_t GetDamageToAbsorb()  const { return m_DamageToAbsorb; }

	/**
	 * Sets the reduced damage value for each attack for this entity, also serializes that change.
	 * @param value the damage to reduce for each attack
	 */
	void SetDamageReduction(int32_t value);

	/**
	 * Returns the current damage reduction value
	 * @return the current damage reduction value
	 */
	int32_t GetDamageReduction() const { return m_DamageReduction; }

	/**
	 * Sets whether or not this entity is immune to attacks
	 * @param value whether or not this entity is immune to attacks
	 */
	void SetIsImmune(bool value);

	/**
	 * Returns whether or not this entity is immune to attacks
	 * @return whether or not this entity is immune to attacks
	 */
	bool IsImmune() const;

	/**
	 * Sets if this entity has GM immunity, making it not killable
	 * @param value the GM immunity of this entity
	 */
	void SetIsGMImmune(bool value);

	/**
	 * Returns whether or not this entity has GM immunity
	 * @return whether or not this entity has GM immunity
	 */
	bool GetIsGMImmune() const { return m_IsGMImmune; }

	/**
	 * Sets whether or not this entity is shielded for a certain amount of damage
	 * @param value whether or not this entity is shielded for a certain amount of damage
	 */
	void SetIsShielded(bool value);

	/**
	 * Returns if this entity is currently shielded from damage
	 * @return if this entity is currently shielded from damage
	 */
	bool GetIsShielded() const { return m_IsShielded; }

	/**
	 * Adds a faction to the faction list of this entity, potentially making more factions friendly. Fetches the info
	 * from the CDClient.
	 * @param factionID the faction ID to add
	 * @param ignoreChecks whether or not to allow factionID -1
	 */
	void AddFaction(int32_t factionID, bool ignoreChecks = false);

	/**
	 * Adds a faction ID to the enemy list
	 * @param factionID the faction ID to make an enemy
	 */
	void AddEnemyFaction(int32_t factionID);

	/**
	 * Sets whether or not this entity can be smashed, does not indicate the smashable glow, which is indicated by
	 * faction ids
	 * @param value whether or not this entity is smashable
	 */
	void SetIsSmashable(bool value);

	/**
	 * Returns whether or not this entity is smashable
	 * @return whether or not this entity is smashable
	 */
	bool GetIsSmashable() const { return m_IsSmashable; }

	/**
	 * Returns the current is-dead value, this is mostly unused
	 * @return the current is-dead value, this is mostly unused
	 */
	bool GetIsDead() const { return m_IsDead; }

	/**
	 * Returns the current is-smashed value, this is mostly unused
	 * @return the current is-smashed value, this is mostly unused
	 */
	bool GetIsSmashed() const { return m_IsSmashed; }

	/**
	 * Sets whether or not this entity has bricks flying out when smashed
	 * @param value whether or not this entity has bricks flying out when smashed
	 */
	void SetHasBricks(bool value);

	/**
	 * Returns whether or not this entity has bricks flying out when smashed
	 * @return whether or not this entity has bricks flying out when smashed
	 */
	bool GetHasBricks() const { return m_HasBricks; }

	/**
	 * Sets the multiplier for the explosion that's visible when the bricks fly out when this entity is smashed
	 * @param value the multiplier for the explosion that's visible when the bricks fly out when this entity is smashed
	 */
	void SetExplodeFactor(float value);

	/**
	 * Returns the current multiplier for explosions
	 * @return the current multiplier for explosions
	 */
	float GetExplodeFactor() const { return m_ExplodeFactor; }

	/**
	 * Sets the amount of attacks this entity can block before being able to be damaged again, useful for example for
	 * shields.
	 * @param value the amount of attacks this entity can block before being able to be damaged again
	 */
	void SetAttacksToBlock(uint32_t value);

	/**
	 * Returns the current amount of attacks this entity can block
	 * @return the current amount of attacks this entity can block
	 */
	uint32_t GetAttacksToBlock() const { return m_AttacksToBlock; }

	/**
	 * Sets whether or not this enemy currently has threats, NOTE: only here for serialization, has no use internally
	 * @param value whether or not this enemy currently has threats
	 */
	void SetHasThreats(bool value);

	/**
	 * Returns whether or not this entity currently has threats, NOTE: unused internally
	 * @return whether or not this entity currently has threats
	 */
	bool GetHasThreats() const { return m_HasThreats; }

	/**
	 * Returns whether or not this entity is knockback immune, based on whether it's quickbuilding or has assembly gear
	 * @return whether or not this entity is knockback immune
	 */
	bool IsKnockbackImmune() const;

	/**
	 * Sets the faction ID of this entity, overriding all previously set entries
	 * @param factionID the faction ID to set
	 */
	void SetFaction(int32_t factionID, bool ignoreChecks = false);

	/**
	 * Returns whether or not the provided entity is an enemy of this entity
	 * @param other the entity to check
	 * @return whether the provided entity is an enemy of this entity or not
	 */
	bool IsEnemy(const Entity* other) const;

	/**
	 * Returns whether or not the provided entity is a friend of this entity
	 * @param other the entity to check
	 * @return whether or not the provided entity is a friend of this entity
	 */
	bool IsFriend(const Entity* other) const;

	/**
	 * Returns all the faction IDs that this entity considers a friend
	 * @return all the faction IDs that this entity considers a friend
	 */
	const std::vector<int32_t>& GetFactionIDs() const { return m_FactionIDs; }

	/**
	 * Returns all the faction IDs that this entity considers an enemy
	 * @return all the faction IDs that this entity considers an enemy
	 */
	const std::vector<int32_t>& GetEnemyFactionsIDs() const { return m_EnemyFactionIDs; }

	/**
	 * Returns whether the provided faction is a friendly faction
	 * @param factionID the faction ID to check
	 * @return whether the provided faction is a friendly faction
	 */
	bool HasFaction(int32_t factionID) const;

	/**
	 * Sets the minimum amount of coins this entity drops when smashed
	 * @param minCoins the minimum amount of coins this entity drops when smashed
	 */
	void SetMinCoins(uint32_t minCoins) { m_MinCoins = minCoins; }

	/**
	 * Returns the minimum amount of coins this entity drops when smashed
	 * @return the minimum amount of coins this entity drops when smashed
	 */
	uint32_t GetMinCoins() const { return m_MinCoins; }

	/**
	 * Sets the maximum amount of coins this entity drops when smashed
	 * @param maxCoins the maximum amount of coins this entity drops when smashed
	 */
	void SetMaxCoins(uint32_t maxCoins) { m_MaxCoins = maxCoins; }

	/**
	 * Returns the maximum amount of coins this entity drops when smashed
	 * @return the maximum amount of coins this entity drops when smashed
	 */
	uint32_t GetMaxCoins() const { return m_MaxCoins; }

	/**
	 * Sets the loot matrix ID that will be used to determine what items to drop when this entity is smashed
	 * @param lootMatrixID the loot matrix ID to set
	 */
	void SetLootMatrixID(uint32_t lootMatrixID) { m_LootMatrixID = lootMatrixID; }

	/**
	 * Returns the current loot matrix ID that will be used to determine loot drops when this entity is smashed
	 * @return the current loot matrix ID
	 */
	uint32_t GetLootMatrixID() const { return m_LootMatrixID; }

	/**
	 * Returns the ID of the entity that killed this entity, if any
	 * @return the ID of the entity that killed this entity, if any
	 */
	LWOOBJID GetKillerID() const;

	/**
	 * Returns the entity that killed this entity, if any
	 * @return the entity that killed this entity, if any
	 */
	Entity* GetKiller() const;

	/**
	 * Checks if the target ID is a valid enemy of this entity
	 * @param target the target ID to check for
	 * @param ignoreFactions whether or not check for the factions, e.g. just return true if the entity cannot be smashed
	 * @return if the target ID is a valid enemy
	 */
	bool CheckValidity(LWOOBJID target, bool ignoreFactions = false, bool targetEnemy = true, bool targetFriend = false) const;

	/**
	 * Attempt to damage this entity, handles everything from health and armor to absorption, immunity and callbacks.
	 * @param damage the damage to attempt to apply
	 * @param source the attacker that caused this damage
	 * @param skillID the skill that damaged this entity
	 * @param echo whether or not to serialize the damage
	 */
	void Damage(uint32_t damage, LWOOBJID source, uint32_t skillID = 0, bool echo = true);

	/**
	 * Smashes this entity, notifying all clients
	 * @param source the source that smashed this entity
	 * @param skillID the skill that killed this entity
	 * @param killType the way this entity was killed, determines if a client animation is played
	 * @param deathType the animation to play when killed
	 */
	void Smash(LWOOBJID source, eKillType killType = eKillType::VIOLENT, const std::u16string& deathType = u"", uint32_t skillID = 0);

	/**
	 * Pushes a layer of immunity to this entity, making it immune for longer
	 * @param stacks the amount of immunity to add
	 */
	void PushImmunity(int32_t stacks = 1);

	/**
	 * Pops layers of immunity, making it immune for less longer
	 * @param stacks the number of layers of immunity to remove
	 */
	void PopImmunity(int32_t stacks = 1);

	/**
	 * Utility to reset all stats to the default stats based on items and completed missions
	 */
	void FixStats();

	/**
	 * Adds a callback that is called when this entity is hit by some other entity
	 * @param callback the callback to add
	 */
	void AddOnHitCallback(const std::function<void(Entity*)>& callback);

private:
	/**
	 * Whether or not the health should be serialized
	 */
	bool m_DirtyHealth;

	/**
	 * The health of the entity
	 */
	int32_t m_iHealth;

	/**
	 * The max health of the entity
	 */
	float m_fMaxHealth;

	/**
	 * The armor of the entity
	 */
	int32_t m_iArmor;

	/**
	 * The max armor of the entity
	 */
	float m_fMaxArmor;

	/**
	 * The imagination of the entity
	 */
	int32_t m_iImagination;

	/**
	 * The max imagination of the entity
	 */
	float m_fMaxImagination;

	/**
	 * The damage this entity can absord before being able to be damaged again
	 */
	int32_t m_DamageToAbsorb;

	/**
	 * Whether this entity currently has GM immunity, making it unsmashable
	 */
	bool m_IsGMImmune;

	/**
	 * Whether this entity is currently shielded from other attacks
	 */
	bool m_IsShielded;

	/**
	 * The number of attacks this entity can block before being able to be attacked again
	 */
	uint32_t m_AttacksToBlock;

	/**
	 * The layers of immunity this entity has left
	 */
	int32_t m_ImmuneStacks;

	/**
	 * The amount of damage that should be reduced from every attack
	 */
	int32_t m_DamageReduction;

	/**
	 * The faction IDs this entity considers friendly
	 */
	std::vector<int32_t> m_FactionIDs;

	/**
	 * The faction IDs this entity considers hostile
	 */
	std::vector<int32_t> m_EnemyFactionIDs;

	/**
	 * Whether this entity is smasahble, mostly unused
	 */
	bool m_IsSmashable;

	/**
	 * Whether this entity is dead. Unused, here for serialization
	 */
	bool m_IsDead;

	/**
	 * Whether this entity is smashed. Unused, here for serialization
	 */
	bool m_IsSmashed;

	/**
	 * Whether this entity has bricks flying out when smashed (causes the client to look up the files)
	 */
	bool m_HasBricks;

	/**
	 * The rate at which bricks fly out when smashed
	 */
	float m_ExplodeFactor;

	/**
	 * Whether the list of potential enemies has changed
	 */
	bool m_DirtyThreatList;

	/**
	 * Whether the entity has threats. Unused: here for serialization
	 */
	bool m_HasThreats;

	/**
	 * The loot matrix that will be used to drop items when the entity is smashed
	 */
	uint32_t m_LootMatrixID;

	/**
	 * The min amount of coins that will drop when this entity is smashed
	 */
	uint32_t m_MinCoins;

	/**
	 * The max amount of coins that will drop when this entity is smashed
	 */
	uint32_t m_MaxCoins;

	/**
	 * The ID of the entity that smashed this entity, if any
	 */
	LWOOBJID m_KillerID;

	/**
	 * The list of callbacks that will be called when this entity gets hit
	 */
	std::vector<std::function<void(Entity*)>> m_OnHitCallbacks;
};

#endif // DESTROYABLECOMPONENT_H
