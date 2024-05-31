#pragma once

#include "RakPeerInterface.h"
#include "dCommonVars.h"
#include "BehaviorBranchContext.h"
#include "GameMessages.h"

#include <vector>
#include <forward_list>

class Behavior;

struct BehaviorSyncEntry
{
	uint32_t handle = 0;

	float time = 0;

	bool ignoreInterrupts = false;

	Behavior* behavior = nullptr;

	BehaviorBranchContext branchContext;

	BehaviorSyncEntry();
};

struct BehaviorTimerEntry
{
	float time = 0;

	Behavior* behavior = nullptr;

	BehaviorBranchContext branchContext;

	LWOOBJID second = LWOOBJID_EMPTY;

	BehaviorTimerEntry();
};

struct BehaviorEndEntry
{
	Behavior* behavior = nullptr;

	uint32_t start = 0;

	BehaviorBranchContext branchContext;

	LWOOBJID second = LWOOBJID_EMPTY;

	BehaviorEndEntry();
};

struct BehaviorContext
{
	LWOOBJID originator = LWOOBJID_EMPTY;

	bool foundTarget = false;

	float skillTime = 0;

	uint32_t skillID = 0;

	uint32_t skillUId = 0;

	bool failed = false;

	bool clientInitalized = false;

	std::vector<BehaviorSyncEntry> syncEntries;

	std::vector<BehaviorTimerEntry> timerEntries;

	std::vector<BehaviorEndEntry> endEntries;

	std::vector<LWOOBJID> scheduledUpdates;

	bool unmanaged = false;

	LWOOBJID caster = LWOOBJID_EMPTY;

	uint32_t GetUniqueSkillId() const;

	void UpdatePlayerSyncs(float deltaTime);

	void RegisterSyncBehavior(uint32_t syncId, Behavior* behavior, const BehaviorBranchContext& branchContext, const float duration, bool ignoreInterrupts = false);

	void RegisterTimerBehavior(Behavior* behavior, const BehaviorBranchContext& branchContext, LWOOBJID second = LWOOBJID_EMPTY);

	void RegisterEndBehavior(Behavior* behavior, const BehaviorBranchContext& branchContext, LWOOBJID second = LWOOBJID_EMPTY);

	void ScheduleUpdate(LWOOBJID id);

	void ExecuteUpdates();

	bool SyncBehavior(uint32_t syncId, RakNet::BitStream& bitStream);

	void Update(float deltaTime);

	void SyncCalculation(uint32_t syncId, float time, Behavior* behavior, const BehaviorBranchContext& branch, bool ignoreInterrupts = false);

	void InvokeEnd(uint32_t id);

	bool CalculateUpdate(float deltaTime);

	void Interrupt();

	void Reset();

	void FilterTargets(std::vector<Entity*>& targetsReference, std::forward_list<int32_t>& ignoreFaction, std::forward_list<int32_t>& includeFaction, const bool targetSelf = false, const bool targetEnemy = true, const bool targetFriend = false, const bool targetTeam = false) const;

	bool CheckTargetingRequirements(const Entity* target) const;

	bool CheckFactionList(std::forward_list<int32_t>& factionList, std::vector<int32_t>& objectsFactions) const;

	explicit BehaviorContext(LWOOBJID originator, bool calculation = false);

	~BehaviorContext();
};
