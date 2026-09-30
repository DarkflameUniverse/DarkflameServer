#include "TacArcBehavior.h"
#include "BehaviorBranchContext.h"
#include "Game.h"
#include "Logger.h"
#include "Entity.h"
#include "BehaviorContext.h"
#include "BaseCombatAIComponent.h"
#include "EntityManager.h"
#include "QuickBuildComponent.h"
#include "DestroyableComponent.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <vector>

void TacArcBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	// TacArcBehavior::Cast (0x00fb2d10): a picked target that passes the filter gets the action and no TacArc data
	if (this->m_usePickedTarget && branch.target != LWOOBJID_EMPTY) {
		std::vector<Entity*> targets = { Game::entityManager->GetEntity(branch.target) };
		context->FilterTargets(targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);
		if (!targets.empty()) {
			this->m_action->Handle(context, bitStream, branch);
			return;
		}
	}

	// Otherwise the client drops the target, so an arc around the target's position has nothing to measure from and
	// writes nothing
	branch.target = LWOOBJID_EMPTY;
	if (this->m_useTargetPostion) return;

	bool hasTargets = false;
	if (!bitStream.Read(hasTargets)) {
		LOG("Unable to read hasTargets from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	};

	if (this->m_checkEnv) {
		bool blocked = false;

		if (!bitStream.Read(blocked)) {
			LOG("Unable to read blocked from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
			return;
		};

		if (blocked) {
			this->m_blockedAction->Handle(context, bitStream, branch);
			return;
		}
	}

	if (hasTargets) {
		std::set<LWOOBJID> targets;
		if (!ReadTargets(bitStream, this->m_maxTargets, targets)) return;

		// The caster wrote the action for each of these targets, so it is read for each, even one that is gone here
		for (const auto target : targets) {
			branch.target = target;
			this->m_action->Handle(context, bitStream, branch);
		}
	} else this->m_missAction->Handle(context, bitStream, branch);
}

bool TacArcBehavior::ReadTargets(RakNet::BitStream& bitStream, const uint32_t maxTargets, std::set<LWOOBJID>& targets) {
	uint32_t count = 0;
	if (!bitStream.Read(count)) {
		LOG("Unable to read count from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return false;
	}

	if (count > maxTargets) {
		LOG("Bitstream has too many targets Max:%i Recv:%i", maxTargets, count);
		return false;
	}

	// TacArcBehavior::DoUnserializeBS (0x00fb26a0) puts the ids in a set: ascending, each once, no empty id
	for (auto i = 0u; i < count; i++) {
		LWOOBJID id{};
		if (!bitStream.Read(id)) {
			LOG("Unable to read id from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
			return false;
		}

		if (id == LWOOBJID_EMPTY) {
			LOG("Bitstream has LWOOBJID_EMPTY as a target!");
			continue;
		}
		targets.insert(id);
	}
	return true;
}

std::set<LWOOBJID> TacArcBehavior::WriteTargets(RakNet::BitStream& bitStream, const std::vector<LWOOBJID>& ordered, const uint32_t maxTargets) {
	// TacArcBehavior::DoHit (0x00fb10c0): the first maxTargets targets, written and acted on in ascending id order
	std::set<LWOOBJID> targets;
	for (const auto target : ordered) {
		if (targets.size() >= maxTargets) break;
		if (target != LWOOBJID_EMPTY) targets.insert(target);
	}

	bitStream.Write<uint32_t>(targets.size());
	for (const auto target : targets) bitStream.Write(target);
	return targets;
}

std::vector<LWOOBJID> TacArcBehavior::OrderTargets(std::vector<Candidate> candidates, const float distanceWeight, const float angleWeight, const float maxRange, const bool useAttackPriority) {
	// GetObjectsInsideTacArc hands the targets over in a set, so they start in ascending id order
	std::ranges::sort(candidates, std::less{}, &Candidate::id);

	// TacArcBehavior::Cast (0x00fb2d10) sorts by distance unless a weight is set
	if (distanceWeight == 0.0f && angleWeight == 0.0f) {
		std::ranges::stable_sort(candidates, std::less{}, &Candidate::distance);
	} else {
		// TacArcBehavior::sortWithWeights (0x00f58cd0): nearer and more straight ahead weighs more, heaviest first
		const auto weight = [=](const Candidate& candidate) {
			const auto distanceScore = maxRange > 0.0f ? (maxRange - candidate.distance) / maxRange : 0.0f;
			const auto angleScore = std::abs(candidate.angle - 180.0f) / 180.0f;
			return distanceWeight * distanceScore + angleWeight * angleScore;
		};
		std::ranges::stable_sort(candidates, std::greater{}, weight);
	}

	// TacArcBehavior::SortByAttackPriority (0x00f72900): buckets by GetAttackPriority, lowest first, each bucket in
	// the order above. Nothing else ranks targets: enemies come before smashables because the enemies'
	// attack_priority (1) is lower than most smashables' (10).
	if (useAttackPriority) std::ranges::stable_sort(candidates, std::less{}, &Candidate::attackPriority);

	std::vector<LWOOBJID> ordered;
	ordered.reserve(candidates.size());
	for (const auto& candidate : candidates) ordered.push_back(candidate.id);
	return ordered;
}

void TacArcBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
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
			this->m_action->Calculate(context, bitStream, branch);
			return;
		}
	}

	// As the client: past the picked target check there is no target
	branch.target = LWOOBJID_EMPTY;

	auto* combatAi = self->GetComponent<BaseCombatAIComponent>();

	const auto casterPosition = self->GetPosition();

	auto reference = self->GetPosition() + m_offset;

	targets.clear();
	std::vector<Candidate> candidates;

	std::vector<Entity*> validTargets = Game::entityManager->GetEntitiesByProximity(reference, this->m_maxRange);

	// filter all valid targets, based on whether we target enemies or friends
	context->FilterTargets(validTargets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);

	for (auto validTarget : validTargets) {
		if (std::find(targets.begin(), targets.end(), validTarget) != targets.end()) continue;
		if (validTarget->GetIsDead()) continue;

		const auto targetPos = validTarget->GetPosition();

		// make sure we aren't too high or low in comparison to the target
		if (targetPos.y > (reference.y + m_upperBound) || targetPos.y < (reference.y + m_lowerBound))
			continue;

		const auto forward = QuatUtils::Forward(self->GetRotation());

		// forward is a normalized vector of where the caster is facing.
		// targetPos is the position of the target.
		// reference is the position of the caster.
		// If we cast a ray forward from the caster, does it come within m_farWidth of the target?

		const auto distance = Vector3::Distance(reference, targetPos);

		if (m_method == 2) {
			NiPoint3 rayPoint = casterPosition + forward * distance;
			if (m_farWidth > 0 && Vector3::DistanceSquared(rayPoint, targetPos) > this->m_farWidth * this->m_farWidth)
				continue;
		}

		auto normalized = (reference - targetPos) / distance;
		const float degreeAngle = std::abs(Vector3::Angle(forward, normalized) * (180 / 3.14) - 180);
		if (distance >= this->m_minRange && this->m_maxRange >= distance && degreeAngle <= 2 * this->m_angle) {
			targets.push_back(validTarget);
			const auto* destroyable = validTarget->GetComponent<DestroyableComponent>();
			candidates.push_back({
				.id = validTarget->GetObjectID(),
				.distance = distance,
				.angle = degreeAngle,
				// An object that does not answer GetAttackPriority keeps the message's default of 1
				.attackPriority = destroyable ? destroyable->GetAttackPriority() : 1,
			});
		}
	}

	// The client keeps the first max targets of this order
	auto ordered = OrderTargets(std::move(candidates), m_distanceWeight, m_angleWeight, m_maxRange, m_useAttackPriority);
	if (m_maxTargets > 0 && ordered.size() > m_maxTargets) ordered.resize(m_maxTargets);
	const auto hit = !ordered.empty();
	bitStream.Write(hit);

	if (this->m_checkEnv) {
		const auto blocked = false; // TODO
		bitStream.Write(blocked);
	}

	if (hit) {
		const auto* first = Game::entityManager->GetEntity(ordered.front());
		if (combatAi && first) combatAi->LookAt(first->GetPosition());

		context->foundTarget = true; // We want to continue with this behavior

		for (const auto target : WriteTargets(bitStream, ordered, this->m_maxTargets)) {
			branch.target = target;
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
	// https://explorer.lu/skills/behaviors/6212/6203 HACK: i cant figure out why the dragon fire wall doesnt work with the offset, probably has to be fixed with the near/far height parameters
	if (m_behaviorId == 6203) {
		this->m_offset = NiPoint3Constant::ZERO;
	}
	this->m_method = GetInt("method", 1);
	this->m_upperBound = GetFloat("upper_bound", 4.4f);
	this->m_lowerBound = GetFloat("lower_bound", 0.4f) - 5.0f; // Makes it so players and objects can still be targetted when slightly below the caster.  FIXME: use bounding spheres at some point
	this->m_usePickedTarget = GetBoolean("use_picked_target", false);
	this->m_useTargetPostion = GetBoolean("use_target_position", false);
	this->m_checkEnv = GetBoolean("check_env", false);
	// TacArcBehavior::Initialize (0x00f9b980): off unless the behavior sets it
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
