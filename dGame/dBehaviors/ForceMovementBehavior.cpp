#include "ForceMovementBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"

void ForceMovementBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch)
{
	if (this->m_hitAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitEnemyAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY && this->m_hitFactionAction->m_templateId == BehaviorTemplates::BEHAVIOR_EMPTY)
	{
		return;
	}

	uint32_t handle;

	bitStream->Read(handle);

	context->RegisterSyncBehavior(handle, this, branch);
}

void ForceMovementBehavior::Sync(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch)
{
	uint32_t next;

	bitStream->Read(next);

	LWOOBJID target;

	bitStream->Read(target);

	auto* behavior = CreateBehavior(next);

	branch.target = target;

	behavior->Handle(context, bitStream, branch);
}

void ForceMovementBehavior::Load()
{
	this->m_hitAction = GetAction("hit_action");

	this->m_hitEnemyAction = GetAction("hit_action_enemy");

	this->m_hitFactionAction = GetAction("hit_action_faction");
}
