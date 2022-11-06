/*
 * Darkflame Universe
 * Copyright 2018
 */

#ifndef SKILLCOMPONENT_H
#define SKILLCOMPONENT_H

#include <map>

#include "BehaviorContext.h"
#include "BitStream.h"
#include "Component.h"
#include "Entity.h"
#include "dLogger.h"

struct ProjectileSyncEntry {
	LWOOBJID id = LWOOBJID_EMPTY;

	bool calculation = false;

	mutable float time = 0;

	float maxTime = 0;

	NiPoint3 startPosition{};

	NiPoint3 lastPosition{};

	NiPoint3 velocity{};

	bool trackTarget = false;

	float trackRadius = 0;

	BehaviorContext* context = nullptr;

	LOT lot = LOT_NULL;

	BehaviorBranchContext branchContext{ 0, 0 };

	explicit ProjectileSyncEntry();
};

struct SkillExecutionResult {
	bool success;

	float skillTime;
};

/**
 * The SkillComponent of an entity. This manages both player and AI skills, such as attacks and consumables.
 * There are two sets of skill methods: one for player skills and one for server-side calculations.
 *
 * Skills are a built up by a tree of behaviors. See dGame/dBehaviors/ for a list of behaviors.
 *
 * This system is very conveluted and still has a lot of unknowns.
 */
class SkillComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_SKILL;

	explicit SkillComponent(Entity* parent);
	~SkillComponent() override;

	static void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Computes skill updates. Invokes CalculateUpdate.
	 */
	void Update(float deltaTime) override;

	/**
	 * Computes server-side skill updates.
	 */
	void CalculateUpdate(float deltaTime);

	/**
	 * Resets all skills, projectiles, and other calculations.
	 */
	void Reset();

	/**
	 * Interrupts active skills.
	 */
	void Interrupt();

	/**
	 * Starts a player skill. Should only be called when the server receives a start skill message from the client.
	 * @param behaviorId the root behavior ID of the skill
	 * @param skillUid the unique ID of the skill given by the client
	 * @param bitStream the bitSteam given by the client to determine the behavior path
	 * @param target the explicit target of the skill
	 */
	bool CastPlayerSkill(uint32_t behaviorId, uint32_t skillUid, RakNet::BitStream* bitStream, LWOOBJID target, uint32_t skillID = 0);

	/**
	 * Continues a player skill. Should only be called when the server receives a sync message from the client.
	 * @param skillUid the unique ID of the skill given by the client
	 * @param syncId the unique sync ID of the skill given by the client
	 * @param bitStream the bitSteam given by the client to determine the behavior path
	 */
	void SyncPlayerSkill(uint32_t skillUid, uint32_t syncId, RakNet::BitStream* bitStream);

	/**
	 * Continues a player projectile calculation. Should only be called when the server receives a projectile sync message from the client.
	 * @param projectileId the unique ID of the projectile given by the client
	 * @param bitStream the bitSteam given by the client to determine the behavior path
	 * @param target the explicit target of the target
	 */
	void SyncPlayerProjectile(LWOOBJID projectileId, RakNet::BitStream* bitStream, LWOOBJID target);

	/**
	 * Registers a player projectile. Should only be called when the server is computing a player projectile.
	 * @param projectileId the unique ID of the projectile given by the client
	 * @param context the current behavior context of the active skill
	 * @param branch the current behavior branch context of the active skill
	 * @param lot the LOT of the projectile
	 */
	void RegisterPlayerProjectile(LWOOBJID projectileId, BehaviorContext* context, const BehaviorBranchContext& branch, LOT lot);

	/**
	 * Initializes a server-side skill calculation.
	 * @param skillId the skill ID
	 * @param behaviorId the root behavior ID of the skill
	 * @param target the explicit target of the skill
	 * @param ignoreTarget continue the skill calculation even if the target is invalid or no target is found
	 * @param clientInitalized indicates if the skill calculation was initiated by a client skill, ignores some checks
	 * @param originatorOverride an override for the originator of the skill calculation
	 * @return the result of the skill calculation
	 */
	SkillExecutionResult CalculateBehavior(uint32_t skillId, uint32_t behaviorId, LWOOBJID target, bool ignoreTarget = false, bool clientInitalized = false, LWOOBJID originatorOverride = LWOOBJID_EMPTY);

	/**
	 * Register a server-side projectile.
	 * @param projectileId the unique ID of the projectile
	 * @param context the current behavior context of the active skill
	 * @param branch the current behavior branch context of the active skill
	 * @param lot the LOT of the projectile
	 * @param maxTime the maximum travel time of the projectile
	 * @param startPosition the start position of the projectile
	 * @param velocity the velocity of the projectile
	 * @param trackTarget whether the projectile should track the target
	 * @param trackRadius the radius of the tracking circle
	 */
	void RegisterCalculatedProjectile(
		LWOOBJID projectileId,
		BehaviorContext* context,
		const BehaviorBranchContext& branch,
		LOT lot,
		const float maxTime,
		const NiPoint3& startPosition,
		const NiPoint3& velocity,
		bool trackTarget,
		float TrackRadius);

	/**
	 * Computes a server-side skill calculation without an associated entity.
	 * @param behaviorId the root behavior ID of the skill
	 * @param target the explicit target of the skill
	 * @param source the explicit source of the skill
	 */
	static void HandleUnmanaged(uint32_t behaviorId, LWOOBJID target, LWOOBJID source = LWOOBJID_EMPTY);

	/**
	 * Computes a server-side skill uncast calculation without an associated entity.
	 * @param behaviorId the root behavior ID of the skill
	 * @param target the explicit target of the skill
	 */
	static void HandleUnCast(uint32_t behaviorId, LWOOBJID target);

	/**
	 * @returns a unique ID for the next skill calculation
	 */
	uint32_t GetUniqueSkillId();

private:
	/**
	 * All of the active skills mapped by their unique ID.
	 */
	std::map<uint32_t, BehaviorContext*> m_managedBehaviors;

	/**
	 * All active projectiles.
	 */
	std::vector<ProjectileSyncEntry> m_managedProjectiles;

	/**
	 * Unique ID counter.
	 */
	uint32_t m_skillUid;

	/**
	 * Sync a server-side projectile calculation.
	 * @param entry the projectile information
	 */
	void SyncProjectileCalculation(const ProjectileSyncEntry& entry) const;
};

#endif // SKILLCOMPONENT_H
