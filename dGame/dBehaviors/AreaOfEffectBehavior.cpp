#include "AreaOfEffectBehavior.h"

#include <vector>

#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "QuickBuildComponent.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "Logger.h"

void AreaOfEffectBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	uint32_t targetCount{};

	if (!bitStream.Read(targetCount)) {
		LOG("Unable to read targetCount from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	}

	if (this->m_useTargetPosition && branch.target == LWOOBJID_EMPTY) return;

	if (targetCount == 0){
		PlayFx(u"miss", context->originator);
		return;
	}

	if (targetCount > this->m_maxTargets) {
		LOG("Serialized size is greater than max targets! Size: %i, Max: %i", targetCount, this->m_maxTargets);
		return;
	}

	auto caster = context->caster;
	if (this->m_useTargetAsCaster) context->caster = branch.target;

	std::vector<LWOOBJID> targets;
	targets.reserve(targetCount);

	for (auto i = 0u; i < targetCount; ++i) {
		LWOOBJID target{};
		if (!bitStream.Read(target)) {
			LOG("failed to read in target %i from bitStream, aborting target Handle!", i);
		};
		targets.push_back(target);
	}

	for (auto target : targets) {
		branch.target = target;
		this->m_action->Handle(context, bitStream, branch);
	}
	context->caster = caster;
	PlayFx(u"cast", context->originator);
}

void AreaOfEffectBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	auto* caster = Game::entityManager->GetEntity(context->caster);
	if (!caster) return;

	// determine the position we are casting the AOE from
	auto reference = branch.isProjectile ? branch.referencePosition : caster->GetPosition();
	if (this->m_useTargetPosition) {
		if (branch.target == LWOOBJID_EMPTY) return;
		auto branchTarget = Game::entityManager->GetEntity(branch.target);
		if (branchTarget) reference = branchTarget->GetPosition();
	}

	reference += this->m_offset;

	std::vector<Entity*> targets {};
	targets = Game::entityManager->GetEntitiesByProximity(reference, this->m_radius);
	context->FilterTargets(targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);

	// sort by distance
	std::sort(targets.begin(), targets.end(), [reference](Entity* a, Entity* b) {
		const auto aDistance = NiPoint3::Distance(a->GetPosition(), reference);
		const auto bDistance = NiPoint3::Distance(b->GetPosition(), reference);
		return aDistance < bDistance;
		}
	);

	// resize if we have more than max targets allows
	if (targets.size() > this->m_maxTargets) targets.resize(this->m_maxTargets);

	bitStream.Write<uint32_t>(targets.size());

	if (targets.size() == 0) {
		PlayFx(u"miss", context->originator);
		return;
	} else {
		context->foundTarget = true;
		// write all the targets to the bitstream
		for (auto* target : targets) {
			bitStream.Write(target->GetObjectID());
		}

		// then cast all the actions
		for (auto* target : targets) {
			branch.target = target->GetObjectID();
			this->m_action->Calculate(context, bitStream, branch);
		}
		PlayFx(u"cast", context->originator);
	}
}

void AreaOfEffectBehavior::Load() {
	this->m_action = GetAction("action"); // required
	this->m_radius = GetFloat("radius", 0.0f); // required
	this->m_maxTargets = GetInt("max targets", 100);
	if (this->m_maxTargets == 0) this->m_maxTargets = 100;
	this->m_useTargetPosition = GetBoolean("use_target_position", false);
	this->m_useTargetAsCaster = GetBoolean("use_target_as_caster", false);
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
