#include "TacArcBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "Logger.h"
#include "Entity.h"
#include "BehaviorContext.h"
#include "BaseCombatAIComponent.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"

#include <vector>

void TacArcBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	std::vector<Entity*> targets = {};

	if (this->m_usePickedTarget && branch.target != LWOOBJID_EMPTY) {
		auto target = Game::entityManager->GetEntity(branch.target);
		if (!target) LOG("target %llu is null", branch.target);
		else {
			targets.push_back(target);
			context->FilterTargets(targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);
			if (!targets.empty()) {
				this->m_action->Handle(context, bitStream, branch);
				return;
			}
		}
	}

	bool hasTargets = false;
	if (!bitStream->Read(hasTargets)) {
		LOG("Unable to read hasTargets from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};

	if (this->m_checkEnv) {
		bool blocked = false;

		if (!bitStream->Read(blocked)) {
			LOG("Unable to read blocked from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};

		if (blocked) {
			this->m_blockedAction->Handle(context, bitStream, branch);
			return;
		}
	}

	if (hasTargets) {
		uint32_t count = 0;
		if (!bitStream->Read(count)) {
			LOG("Unable to read count from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};

		if (count > m_maxTargets) {
			LOG("Bitstream has too many targets Max:%i Recv:%i", this->m_maxTargets, count);
			return;
		}

		for (auto i = 0u; i < count; i++) {
			LWOOBJID id{};

			if (!bitStream->Read(id)) {
				LOG("Unable to read id from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
				return;
			};

			if (id != LWOOBJID_EMPTY) {
				auto* canidate = Game::entityManager->GetEntity(id);
				if (canidate) targets.push_back(canidate);
			} else {
				LOG("Bitstream has LWOOBJID_EMPTY as a target!");
			}
		}

		for (auto target : targets) {
			branch.target = target->GetObjectID();
			this->m_action->Handle(context, bitStream, branch);
		}
	} else this->m_missAction->Handle(context, bitStream, branch);
}

void TacArcBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* self = Game::entityManager->GetEntity(context->originator);
	if (self == nullptr) {
		LOG("Invalid self for (%llu)!", context->originator);
		return;
	}

	std::vector<Entity*> targets = {};
	if (this->m_usePickedTarget && branch.target != LWOOBJID_EMPTY) {
		auto target = Game::entityManager->GetEntity(branch.target);
		targets.push_back(target);
		context->FilterTargets(targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);
		if (!targets.empty()) {
			this->m_action->Handle(context, bitStream, branch);
			return;
		}
	}

	auto* combatAi = self->GetComponent<BaseCombatAIComponent>();

	const auto casterPosition = self->GetPosition();

	auto reference = self->GetPosition(); //+ m_offset;

	targets.clear();

	std::vector<Entity*> validTargets = Game::entityManager->GetEntitiesByProximity(reference, this->m_maxRange);

	// filter all valid targets, based on whether we target enemies or friends
	context->FilterTargets(validTargets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);

	for (auto validTarget : validTargets) {
		if (targets.size() >= this->m_maxTargets) {
			break;
		}

		if (std::find(targets.begin(), targets.end(), validTarget) != targets.end()) {
			continue;
		}

		if (validTarget->GetIsDead()) continue;

		const auto otherPosition = validTarget->GetPosition();

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

		if (distance >= this->m_minRange && this->m_maxRange >= distance && degreeAngle <= 2 * this->m_angle) {
			targets.push_back(validTarget);
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
	this->m_maxRange = GetFloat("max range");
	this->m_height = GetFloat("height", 2.2f);
	this->m_distanceWeight = GetFloat("distance_weight", 0.0f);
	this->m_angleWeight = GetFloat("angle_weight", 0.0f);
	this->m_angle = GetFloat("angle", 45.0f);
	this->m_minRange = GetFloat("min range", 0.0f);
	this->m_offset = NiPoint3(
		GetFloat("offset_x", 0.0f),
		GetFloat("offset_y", 0.0f),
		GetFloat("offset_z", 0.0f)
	);
	this->m_method = GetInt("method", 1);
	this->m_upperBound = GetFloat("upper_bound", 4.4f);
	this->m_lowerBound = GetFloat("lower_bound", 0.4f);
	this->m_usePickedTarget = GetBoolean("use_picked_target", false);
	this->m_useTargetPostion = GetBoolean("use_target_position", false);
	this->m_checkEnv = GetBoolean("check_env", false);
	this->m_useAttackPriority = GetBoolean("use_attack_priority", false);

	this->m_action = GetAction("action");
	this->m_missAction = GetAction("miss action");
	this->m_blockedAction = GetAction("blocked action");

	this->m_maxTargets = GetInt("max targets", 100);
	if (this->m_maxTargets == 0) this->m_maxTargets = 100;

	this->m_farHeight = GetFloat("far_height", 5.0f);
	this->m_farWidth = GetFloat("far_width", 5.0f);
	this->m_nearHeight = GetFloat("near_height", 5.0f);
	this->m_nearWidth = GetFloat("near_width", 5.0f);

	// params after this are needed for filter targets
	const auto parameters = GetParameterNames();
	for (const auto& parameter : parameters) {
		if (parameter.first.rfind("include_faction", 0) == 0) {
			this->m_includeFactionList.push_front(parameter.second);
		} else if (parameter.first.rfind("ignore_faction", 0) == 0) {
			this->m_ignoreFactionList.push_front(parameter.second);
		}
	}
	this->m_targetSelf = GetBoolean("target_caster", false);
	this->m_targetEnemy = GetBoolean("target_enemy", false);
	this->m_targetFriend = GetBoolean("target_friend", false);
	this->m_targetTeam = GetBoolean("target_team", false);
}
