#include "AreaOfEffectBehavior.h"

#include <vector>

#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"

void AreaOfEffectBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	uint32_t targetCount;

	bitStream->Read(targetCount);

	if (targetCount > this->m_maxTargets) {
		return;
	}

	std::vector<LWOOBJID> targets;

	targets.reserve(targetCount);

	for (auto i = 0u; i < targetCount; ++i) {
		LWOOBJID target;

		bitStream->Read(target);

		targets.push_back(target);
	}

	for (auto target : targets) {
		branch.target = target;

		this->m_action->Handle(context, bitStream, branch);
	}
}

void AreaOfEffectBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* self = EntityManager::Instance()->GetEntity(context->caster);

	if (self == nullptr) {
		Game::logger->Log("TacArcBehavior", "Invalid self for (%llu)!", context->originator);

		return;
	}

	auto reference = branch.isProjectile ? branch.referencePosition : self->GetPosition();

	std::vector<Entity*> targets;

	auto* presetTarget = EntityManager::Instance()->GetEntity(branch.target);

	if (presetTarget != nullptr) {
		if (this->m_radius * this->m_radius >= Vector3::DistanceSquared(reference, presetTarget->GetPosition())) {
			targets.push_back(presetTarget);
		}
	}

	int32_t includeFaction = m_includeFaction;

	if (self->GetLOT() == 14466) // TODO: Fix edge case
	{
		includeFaction = 1;
	}

	// Gets all of the valid targets, passing in if should target enemies and friends
	for (auto validTarget : context->GetValidTargets(m_ignoreFaction, includeFaction, m_TargetSelf == 1, m_targetEnemy == 1, m_targetFriend == 1)) {
		auto* entity = EntityManager::Instance()->GetEntity(validTarget);

		if (entity == nullptr) {
			Game::logger->Log("TacArcBehavior", "Invalid target (%llu) for (%llu)!", validTarget, context->originator);

			continue;
		}

		if (std::find(targets.begin(), targets.end(), entity) != targets.end()) {
			continue;
		}

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr) {
			continue;
		}

		if (destroyableComponent->HasFaction(m_ignoreFaction)) {
			continue;
		}

		const auto distance = Vector3::DistanceSquared(reference, entity->GetPosition());

		if (this->m_radius * this->m_radius >= distance && (this->m_maxTargets == 0 || targets.size() < this->m_maxTargets)) {
			targets.push_back(entity);
		}
	}

	std::sort(targets.begin(), targets.end(), [reference](Entity* a, Entity* b) {
		const auto aDistance = Vector3::DistanceSquared(a->GetPosition(), reference);
		const auto bDistance = Vector3::DistanceSquared(b->GetPosition(), reference);

		return aDistance > bDistance;
		});

	const uint32_t size = targets.size();

	bitStream->Write(size);

	if (size == 0) {
		return;
	}

	context->foundTarget = true;

	for (auto* target : targets) {
		bitStream->Write(target->GetObjectID());

		PlayFx(u"cast", context->originator, target->GetObjectID());
	}

	for (auto* target : targets) {
		branch.target = target->GetObjectID();

		this->m_action->Calculate(context, bitStream, branch);
	}
}

void AreaOfEffectBehavior::Load() {
	this->m_action = GetAction("action");

	this->m_radius = GetFloat("radius");

	this->m_maxTargets = GetInt("max targets");

	this->m_ignoreFaction = GetInt("ignore_faction");

	this->m_includeFaction = GetInt("include_faction");

	this->m_TargetSelf = GetInt("target_self");

	this->m_targetEnemy = GetInt("target_enemy");

	this->m_targetFriend = GetInt("target_friend");
}
