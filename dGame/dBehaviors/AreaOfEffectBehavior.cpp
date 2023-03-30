#include "AreaOfEffectBehavior.h"

#include <vector>

#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "dLogger.h"

void AreaOfEffectBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t targetCount{};

	if (!bitStream->Read(targetCount)) {
		Game::logger->Log("AreaOfEffectBehavior", "Unable to read targetCount from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	}

	if (targetCount > this->m_maxTargets) {
		Game::logger->Log("AreaOfEffectBehavior::Handle", "More targets than allowed! Likely a cheating attempt. Got: %i, Max: %i", targetCount, this->m_maxTargets);
		return;
	}

	std::vector<LWOOBJID> targets;
	targets.reserve(targetCount);

	for (auto i = 0u; i < targetCount; ++i) {
		LWOOBJID target{};
		if (!bitStream->Read(target)) {
			Game::logger->Log("AreaOfEffectBehavior", "failed to read in target %i from bitStream, aborting target Handle!", i);
			return;
		};
		targets.push_back(target);
	}

	for (auto target : targets) {
		branch.target = target;
		this->m_action->Handle(context, bitStream, branch);
	}
}

void AreaOfEffectBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* caster = EntityManager::Instance()->GetEntity(context->caster);
	if (!caster) {
		Game::logger->Log("AreaOfEffectBehavior::Calculate", "There is no caster to be found!");
		return;
	}

	// determine the position we are casting the AOE from
	auto reference = branch.isProjectile ? branch.referencePosition : caster->GetPosition();
	if (m_UseTargetPosition && branch.target) {
		auto branchTarget = EntityManager::Instance()->GetEntity(branch.target);
		reference = branchTarget->GetPosition();
	}

	reference += this->m_offset;

	std::vector<Entity*> allTargets {};
	std::vector<Entity*> validTargets {};
	// Gets all of the valid validTargets, passing in if should target enemies and friends
	allTargets = context->FilterTargets(this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);

	// filter based on the radius
	for (auto* candidate : allTargets) if (NiPoint3::Distance(reference, candidate->GetPosition()) <= this->m_radius) validTargets.push_back(candidate);

	// sort by distance
	std::sort(validTargets.begin(), validTargets.end(), [reference](Entity* a, Entity* b) {
		const auto aDistance = NiPoint3::Distance(a->GetPosition(), reference);
		const auto bDistance = NiPoint3::Distance(b->GetPosition(), reference);
		return aDistance < bDistance;
		}
	);

	// resize if we have more than max validTargets allows
	if (validTargets.size() > this->m_maxTargets) validTargets.resize(this->m_maxTargets);

	bitStream->Write<uint32_t>(validTargets.size());

	if (validTargets.size() > 0) context->foundTarget = true;

	// write all the targets to the bitstream
	for (auto* validTarget : validTargets) {
		bitStream->Write(validTarget->GetObjectID());
	}

	// then case all the actions
	for (auto* validTarget : validTargets) {
		bitStream->Write(validTarget->GetObjectID());
		branch.target = validTarget->GetObjectID();
		this->m_action->Calculate(context, bitStream, branch);
	}

	PlayFx(u"cast", context->originator);
}

void AreaOfEffectBehavior::Load() {

	this->m_action = GetAction("action");
	this->m_radius = GetFloat("radius");
	this->m_maxTargets = GetInt("max targets", 100);
	if (this->m_maxTargets == 0) this->m_maxTargets = 100;
	this->m_offset = NiPoint3(
		GetFloat("offset_x", 0.0f),
		GetFloat("offset_y", 0.0f),
		GetFloat("offset_z", 0.0f)
	);
	// params after this are needed for filter targets
	const auto parameters = GetParameterNames();
	for (const auto& parameter : parameters) {
		if (parameter.first.rfind("include_faction", 0) == 0) {
			this->m_includeFactionList.push_front(parameter.second);
		} else if (parameter.first.rfind("ignore_faction", 0) == 0) {
			this->m_ignoreFactionList.push_front(parameter.second);
		}
	}
	this->m_targetSelf = GetBoolean("target_self", false);
	this->m_targetEnemy = GetBoolean("target_enemy", false);
	this->m_targetFriend = GetBoolean("target_friend", false);
	this->m_targetTeam = GetBoolean("target_team", false);


}
