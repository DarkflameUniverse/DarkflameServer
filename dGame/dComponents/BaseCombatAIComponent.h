#ifndef BASECOMBATAICOMPONENT_H
#define BASECOMBATAICOMPONENT_H

#include "RakNetTypes.h"
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "Behavior.h"
#include "dpWorld.h"
#include "dpEntity.h"
#include "Component.h"

#include <vector>
#include <map>

class MovementAIComponent;
class Entity;

/**
 * The current state of the AI
 */
enum class AiState : int {
	idle = 0,   // Doing nothing
	aggro,      // Waiting for an enemy to cross / running back to spawn
	tether,     // Chasing an enemy
	spawn,      // Spawning into the world
	dead        // Killed
};

/**
 * Represents a skill that can be cast by this enemy, including its cooldowns, which determines how often the skill
 * may be cast.
 */
struct AiSkillEntry
{
	uint32_t skillId;

	float cooldown;

	float abilityCooldown;

	Behavior* behavior;
};

/**
 * Handles the AI of entities, making them wander, tether and attack their enemies
 */
class BaseCombatAIComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_BASE_COMBAT_AI;

	BaseCombatAIComponent(Entity* parentEntity, uint32_t id);
	~BaseCombatAIComponent() override;

	void Update(float deltaTime) override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Get the current behavioral state of the enemy
	 * @return the current state
	 */
	AiState GetState() const { return m_State; }

	/**
	 * Set the current behavioral state of the enemy
	 * @param state the state to change to
	 */
	void SetState(AiState state) { m_State = state; }

	/**
	 * Checks if the target may be an enemy of this entity
	 * @param target the target to check for
	 * @return whether the target is a valid enemy for this entity or not
	 */
	bool IsEnemy(LWOOBJID target) const;

	/**
	 * Gets the current target ID that this entity will attack
	 * @return the current target ID of this entity
	 */
	LWOOBJID GetTarget() const { return m_Target; }

	/**
	 * Sets the target that this entity will attack
	 * @param target the target to set
	 */
	void SetTarget(LWOOBJID target);

	/**
	 * Gets the current target entity that this entity will attack
	 * @return the current target entity of this entity
	 */
	Entity* GetTargetEntity() const;

	/**
	 * Taunts this entity, making it a higher or lower threat for this entity. Increasing or decreasing the chance to
	 * be attacked.
	 * @param offender the entity that triggered the taunt
	 * @param threat how high to increase the threat for the offender
	 */
	void Taunt(LWOOBJID offender, float threat);

	/**
	 * Gets the current threat level for an offending entity
	 * @param offender the entity to get the threat for
	 * @return the current threat level of the offending entity, 0 if the entity is not a threat
	 */
	float GetThreat(LWOOBJID offender);

	/**
	 * Sets the threat level for an entity
	 * @param offender the entity to set the threat level for
	 * @param threat the threat level to set
	 */
	void SetThreat(LWOOBJID offender, float threat);

	/**
	 * Gets the position where the entity spawned
	 * @return the position where the entity spawned
	 */
	const NiPoint3& GetStartPosition() const;

	/**
	 * Removes all threats for this entities, and thus chances for it attacking other entities
	 */
	void ClearThreat();

	/**
	 * Makes the entity continue to wander to a random point around it's starting position
	 */
	void Wander();

	/**
	 * Continues a step in the aggro state, making sure that the entity is around its start position, if an entity
	 * crosses its aggro range this will set the state to tether.
	 */
	void OnAggro();

	/**
	 * Continues a step in the tether state, making the entity run towards its target, if the target is outside of its
	 * tether range, this will change the state to aggro
	 */
	void OnTether();

	/**
	 * Gets whether or not the entity is currently stunned
	 * @return whether the entity is currently stunned
	 */
	bool GetStunned() const;

	/**
	 * (un)stuns the entity, determining whether it'll be able to attack other entities
	 * @param value whether the enemy is stunned
	 */
	void SetStunned(bool value);

	/**
	 * Gets if this entity may be stunned
	 * @return if this entity may be stunned
	 */
	bool GetStunImmune() const;

	/**
	 * Set the stun immune value, determining if the entity may be stunned
	 * @param value
	 */
	void SetStunImmune(bool value);

	/**
	 * Gets the current speed at which an entity runs when tethering
	 * @return the current speed at which an entity runs when tethering
	 */
	float GetTetherSpeed() const;

	/**
	 * Sets the speed at which an entity will tether
	 * @param value the new tether speed
	 */
	void SetTetherSpeed(float value);

	/**
	 * Stuns the entity for a certain amount of time, will not work if the entity is stun immune
	 * @param time the time to stun the entity, if stunnable
	 */
	void Stun(float time);

	/**
	 * Gets the radius that will cause this entity to get aggro'd, causing a target chase
	 * @return the aggro radius of the entity
	 */
	float GetAggroRadius() const;

	/**
	 * Sets the aggro radius, causing the entity to start chasing enemies in this range
	 * @param value the aggro radius to set
	 */
	void SetAggroRadius(float value);

	/**
	 * Makes the entity look at a certain point in space
	 * @param point the point to look at
	 */
	void LookAt(const NiPoint3& point);

	/**
	 * (dis)ables the AI, causing it to stop/start attacking enemies
	 * @param value
	 */
	void SetDisabled(bool value);

	/**
	 * Gets the current state of the AI, whether or not it's looking for enemies to attack
	 * @return
	 */
	bool GetDistabled() const;

	/**
	 * Turns the entity asleep, stopping updates to its physics volumes
	 */
	void Sleep();

	/**
	 * Wakes the entity, allowing updates to its physics volumes
	 */
	void Wake();

private:
	/**
	 * Returns the current target or the target that currently is the largest threat to this entity
	 * @return the current highest priority enemy of this entity
	 */
	LWOOBJID FindTarget();

	/**
	 * Handles anything attack related for the game loop, e.g.: finding targets, sticking with targets and attacking
	 * them, depending on cooldowns.
	 * @param deltaTime the time since the last game tick
	 */
	void CalculateCombat(float deltaTime);

	/**
	 * Gets all the targets that are in the aggro collision phantom of this entity
	 * @return the targets within the aggro range of this entity
	 */
	std::vector<LWOOBJID> GetTargetWithinAggroRange() const;

	/**
	 * The current state of the AI
	 */
	AiState m_State;

	/**
	 * The target this entity is currently trying to attack
	 */
	LWOOBJID m_Target;

	/**
	 * The aggro physics volumes of this entity
	 */
	dpEntity* m_dpEntity;
	dpEntity* m_dpEntityEnemy;

	/**
	 * The max radius of this entity to an enemy allowing it to be chased
	 */
	float m_HardTetherRadius = 100;

	/**
	 * A soft radius for the tether, currently unused
	 */
	float m_SoftTetherRadius = 25;

	/**
	 * The speed at which this entity chases enemies
	 */
	float m_PursuitSpeed = 2;

	/**
	 * The radius that can cause enemies to aggro this entity
	 */
	float m_AggroRadius = 25;

	/**
	 * The speed at which an enemy wanders around
	 */
	float m_TetherSpeed = 4;

	/**
	 * How close this entity needs to be to an enemy to allow attacks
	 */
	float m_AttackRadius = 5.0f;

	/**
	 * Timer before we start attacking others
	 */
	float m_Timer = 0.0f;

	/**
	 * Timer to serializing this entity
	 */
	float m_SoftTimer = 0.0f;

	/**
	 * The skills this entity can cast on enemies
	 */
	std::vector<AiSkillEntry> m_SkillEntries;

	/**
	 * The current enemies and their respective threats to this entity
	 */
	std::map<LWOOBJID, float> m_ThreatEntries;

	/**
	 * The component that handles movement AI, also owned by this entity
	 */
	MovementAIComponent* m_MovementAI;

	/**
	 * The position at which this entity spawned
	 */
	NiPoint3 m_StartPosition;

	/**
	 * For how long this entity has been stunned
	 */
	float m_StunTime = 0;

	/**
	 * If this entity is stunned
	 */
	bool m_Stunned = false;

	/**
	 * If this entity is immune to stunds
	 */
	bool m_StunImmune = false;

	/**
	 * Time taken between actions
	 */
	float m_Downtime = 0;

	/**
	 * How long this entity needs to execute its skill
	 */
	float m_SkillTime = 0;

	/**
	 * If the entity is currently showing the exclamation mark icon above its head
	 */
	bool m_TetherEffectActive = false;

	/**
	 * How long the tether effect will remain active
	 */
	float m_TetherTime = 0;

	/**
	 * How long until we will consider this entity out of combat, resetting its health and armor
	 */
	float m_OutOfCombatTime = 0;

	/**
	 * If the entity is currently out of combat, resetting its health and armor if it just came out of combat
	 */
	bool m_OutOfCombat = false;

	/**
	 * If the AI is currently disabled
	 */
	bool m_Disabled = false;

	/**
	 * If the threat list should be updated
	 */
	bool m_DirtyThreat = false;

	/**
	 * Whether the current entity is a mech enemy, needed as mechs tether radius works differently
	 * @return whether this entity is a mech
	 */
	bool IsMech();
};

#endif // BASECOMBATAICOMPONENT_H
