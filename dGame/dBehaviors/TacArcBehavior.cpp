#include "TacArcBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "dLogger.h"
#include "Entity.h"
#include "BehaviorContext.h"
#include "BaseCombatAIComponent.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"

#include <vector>

void TacArcBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (this->m_targetEnemy && this->m_usePickedTarget && branch.target > 0) {
		this->m_action->Handle(context, bitStream, branch);

		return;
	}

	bool hit = false;

	bitStream->Read(hit);

	if (this->m_checkEnv) {
		bool blocked = false;

		bitStream->Read(blocked);

		if (blocked) {
			this->m_blockedAction->Handle(context, bitStream, branch);

			return;
		}
	}

	if (hit) {
		uint32_t count = 0;

		bitStream->Read(count);

		if (count > m_maxTargets && m_maxTargets > 0) {
			count = m_maxTargets;
		}

		std::vector<LWOOBJID> targets;

		for (auto i = 0u; i < count; ++i) {
			LWOOBJID id;

			bitStream->Read(id);

			targets.push_back(id);
		}

		for (auto target : targets) {
			branch.target = target;

			this->m_action->Handle(context, bitStream, branch);
		}
	} else {
		this->m_missAction->Handle(context, bitStream, branch);
	}
}

void TacArcBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* self = EntityManager::Instance()->GetEntity(context->originator);
	if (self == nullptr) {
		Game::logger->Log("TacArcBehavior", "Invalid self for (%llu)!", context->originator);
		return;
	}

	const auto* destroyableComponent = self->GetComponent<DestroyableComponent>();

	if ((this->m_usePickedTarget || context->clientInitalized) && branch.target > 0) {
		const auto* target = EntityManager::Instance()->GetEntity(branch.target);

		if (target == nullptr) {
			return;
		}

		// If the game is specific about who to target, check that
		if (destroyableComponent == nullptr || ((!m_targetFriend && !m_targetEnemy
			|| m_targetFriend && destroyableComponent->IsFriend(target)
			|| m_targetEnemy && destroyableComponent->IsEnemy(target)))) {
			this->m_action->Calculate(context, bitStream, branch);
		}

		return;
	}

	auto* combatAi = self->GetComponent<BaseCombatAIComponent>();

	const auto casterPosition = self->GetPosition();

	auto reference = self->GetPosition(); //+ m_offset;

	std::vector<Entity*> targets;

	std::vector<LWOOBJID> validTargets;

	if (combatAi != nullptr) {
		if (combatAi->GetTarget() != LWOOBJID_EMPTY) {
			validTargets.push_back(combatAi->GetTarget());
		}
	}

	// Find all valid targets, based on whether we target enemies or friends
	for (const auto& contextTarget : context->GetValidTargets()) {
		if (destroyableComponent != nullptr) {
			const auto* targetEntity = EntityManager::Instance()->GetEntity(contextTarget);

			if (m_targetEnemy && destroyableComponent->IsEnemy(targetEntity)
				|| m_targetFriend && destroyableComponent->IsFriend(targetEntity)) {
				validTargets.push_back(contextTarget);
			}
		} else {
			validTargets.push_back(contextTarget);
		}
	}

	for (auto validTarget : validTargets) {
		if (targets.size() >= this->m_maxTargets) {
			break;
		}

		auto* entity = EntityManager::Instance()->GetEntity(validTarget);

		if (entity == nullptr) {
			Game::logger->Log("TacArcBehavior", "Invalid target (%llu) for (%llu)!", validTarget, context->originator);

			continue;
		}

		if (std::find(targets.begin(), targets.end(), entity) != targets.end()) {
			continue;
		}

		if (entity->GetIsDead()) continue;

		const auto otherPosition = entity->GetPosition();

		const auto heightDifference = std::abs(otherPosition.y - casterPosition.y);

		/*if (otherPosition.y > reference.y && heightDifference > this->m_upperBound || otherPosition.y < reference.y && heightDifference > this->m_lowerBound)
		{
			continue;
		}*/

		const auto forward = self->GetRotation().GetForwardVector();

		// forward is a normalized vector of where the caster is facing.
		// otherPosition is the position of the target.
		// reference is the position of the caster.
		// If we cast a ray forward from the caster, does it come within m_farWidth of the target?

		const auto distance = Vector3::Distance(reference, otherPosition);

		if (m_method == 2) {
			NiPoint3 rayPoint = casterPosition + forward * distance;

			if (m_farWidth > 0 && Vector3::DistanceSquared(rayPoint, otherPosition) > this->m_farWidth * this->m_farWidth) {
				continue;
			}
		}

		auto normalized = (reference - otherPosition) / distance;

		const float degreeAngle = std::abs(Vector3::Angle(forward, normalized) * (180 / 3.14) - 180);

		if (distance >= this->m_minDistance && this->m_maxDistance >= distance && degreeAngle <= 2 * this->m_angle) {
			targets.push_back(entity);
		}
	}

	std::sort(targets.begin(), targets.end(), [reference](Entity* a, Entity* b) {
		const auto aDistance = Vector3::DistanceSquared(reference, a->GetPosition());
		const auto bDistance = Vector3::DistanceSquared(reference, b->GetPosition());

		return aDistance > bDistance;
		});

	const auto hit = !targets.empty();

	bitStream->Write(hit);

	if (this->m_checkEnv) {
		const auto blocked = false; // TODO

		bitStream->Write(blocked);
	}

	if (hit) {
		if (combatAi != nullptr) {
			combatAi->LookAt(targets[0]->GetPosition());
		}

		context->foundTarget = true; // We want to continue with this behavior

		const auto count = static_cast<uint32_t>(targets.size());

		bitStream->Write(count);

		for (auto* target : targets) {
			bitStream->Write(target->GetObjectID());
		}

		for (auto* target : targets) {
			branch.target = target->GetObjectID();

			this->m_action->Calculate(context, bitStream, branch);
		}
	} else {
		this->m_missAction->Calculate(context, bitStream, branch);
	}
}

void TacArcBehavior::Load() {
	this->m_usePickedTarget = GetBoolean("use_picked_target");

	this->m_action = GetAction("action");

	this->m_missAction = GetAction("miss action");

	this->m_checkEnv = GetBoolean("check_env");

	this->m_blockedAction = GetAction("blocked action");

	this->m_minDistance = GetFloat("min range");

	this->m_maxDistance = GetFloat("max range");

	this->m_maxTargets = GetInt("max targets");

	this->m_targetEnemy = GetBoolean("target_enemy");

	this->m_targetFriend = GetBoolean("target_friend");

	this->m_targetTeam = GetBoolean("target_team");

	this->m_angle = GetFloat("angle");

	this->m_upperBound = GetFloat("upper_bound");

	this->m_lowerBound = GetFloat("lower_bound");

	this->m_farHeight = GetFloat("far_height");

	this->m_farWidth = GetFloat("far_width");

	this->m_method = GetInt("method");

	this->m_offset = {
		GetFloat("offset_x"),
		GetFloat("offset_y"),
		GetFloat("offset_z")
	};
}
