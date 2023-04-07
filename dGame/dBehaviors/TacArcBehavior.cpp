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
#include "TacArcInfo.h"
#include "eTacArcMethod.h"

#include <vector>

void TacArcBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* caster = EntityManager::Instance()->GetEntity(context->caster);
	if (!caster) return;

	std::vector<Entity*> targets = {};
	if (this->m_usePickedTarget && branch.target != LWOOBJID_EMPTY){
		auto target = EntityManager::Instance()->GetEntity(branch.target);
		targets.push_back(target);
		context->FilterTargets(targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);
		if(!targets.empty()) {
			this->m_action->Handle(context, bitStream, branch);
			return;
		}
	}

	bool hasTargets = false;
	if (!bitStream->Read(hasTargets)) {
		Game::logger->Log("TacArcBehavior", "Unable to read hasTargets from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};

	if (this->m_checkEnv) {
		bool blocked = false;

		if (!bitStream->Read(blocked)) {
			Game::logger->Log("TacArcBehavior", "Unable to read blocked from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
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
			Game::logger->Log("TacArcBehavior", "Unable to read count from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};

		if (count > m_maxTargets) {
			Game::logger->Log("TacArcBehavior", "TacArcBehavior Bitstream too many targets Max:%i Recv:%i", this->m_maxTargets, count);
			return;
		}

		for (auto i = 0u; i < count; i++) {
			LWOOBJID id{};

			if (!bitStream->Read(id)) {
				Game::logger->Log("TacArcBehavior", "Unable to read id from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
				return;
			};

			if (id != LWOOBJID_EMPTY) {
				auto* canidate = EntityManager::Instance()->GetEntity(id);
				targets.push_back(canidate);
			} else {
				Game::logger->Log("TacArcBehavior", "Bitstream has LWOOBJID_EMPTY as a target!");
			}
		}

		for (auto target : targets) {
			branch.target = target->GetObjectID();
			this->m_action->Handle(context, bitStream, branch);
		}
	} else this->m_missAction->Handle(context, bitStream, branch);
}

void TacArcBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto* caster = EntityManager::Instance()->GetEntity(context->caster);
	if (!caster) return;

	if (this->m_usePickedTarget && branch.target != LWOOBJID_EMPTY){
		std::vector<Entity*> targets = {};
		auto target = EntityManager::Instance()->GetEntity(branch.target);
		if (target) targets.push_back(target);
		context->FilterTargets(targets,this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);
		if(!targets.empty()) {
			this->m_action->Calculate(context, bitStream, branch);
			return;
		}
	}

	if (this->m_useTargetPostion) return;
	branch.target = LWOOBJID_EMPTY;

	auto reference = caster->GetPosition();
	reference += this->m_offset;
	auto forward = caster->GetRotation().GetForwardVector();

	auto tacArcInfo = new TacArcInfo();
	tacArcInfo->position = reference;
	tacArcInfo->forwardVector = forward;
	tacArcInfo->weight = 0.0;
	tacArcInfo->angleInDegrees = (this->m_angle * PI) / 180.0;
	tacArcInfo->minRange = this->m_minRange;
	tacArcInfo->maxRange = this->m_maxRange;
	tacArcInfo->farWidth = this->m_farWidth;
	tacArcInfo->farHeight = this->m_farHeight;
	tacArcInfo->nearWidth = this->m_nearWidth;
	tacArcInfo->nearHeight = this->m_nearHeight;
	tacArcInfo->addRadiusToPosition = this->m_method == eTacArcMethod::CONE_AND_PIE_SLICE;
	tacArcInfo->bGetHeight = this->m_method == eTacArcMethod::CONE_AND_PIE_SLICE;
	tacArcInfo->heightLowerBound = reference.y + this->m_lowerBound;
	tacArcInfo->heightUpperBound = reference.y + this->m_upperBound;
	tacArcInfo->method = this->m_method;
	tacArcInfo->targets = {};

	EntityManager::Instance()->GetEntitiesInsideTacArc(*tacArcInfo);
	context->FilterTargets(tacArcInfo->targets, this->m_ignoreFactionList, this->m_includeFactionList, this->m_targetSelf, this->m_targetEnemy, this->m_targetFriend, this->m_targetTeam);

	if (tacArcInfo->targets.size() == 0) {
		// DoMiss
		bitStream->Write0();
		if (this->m_checkEnv){
			reference.y += this->m_height;
			forward *= this->m_maxRange;
			forward += reference;
			if(false /*DoEnvRaycast(reference, forward)*/){
				bitStream->Write1();
				this->m_blockedAction->Calculate(context, bitStream, branch);
				PlayFx(u"blocked", context->caster);
			} else{
				bitStream->Write0();
				this->m_missAction->Calculate(context, bitStream, branch);
				PlayFx(u"miss", context->caster);
			}
		} else this->m_missAction->Calculate(context, bitStream, branch);
	} else {
		bitStream->Write1();
		PlayFx(u"cast", context->caster);
		if (this->m_distanceWeight == 0.0 && this->m_angleWeight == 0.0) {
			// SortNoWeights()
		} else{
			// SortUseWeights()
		}

		if (this->m_useAttackPriority){
			// sort by attack_priority highest first (TODO verify this is the correct order)
			std::sort(tacArcInfo->targets.begin(), tacArcInfo->targets.end(), [reference](Entity* a, Entity* b) {
					return a->GetComponent<DestroyableComponent>()->GetAttackPriority() > b->GetComponent<DestroyableComponent>()->GetAttackPriority();
				}
			);
		}
		if (this->m_checkEnv){
			// DoEnvCheck
			reference.y += this->m_height;
			bool check = true;
			for (auto* validTarget : tacArcInfo->targets) {
				auto targetPosition = validTarget->GetPosition();
				targetPosition.y += this->m_height;
				if( false /*DoEnvRaycast(reference, forward)*/){
					for (auto* validTarget2 : tacArcInfo->targets) {

					}
				}
			}
			if(check){
				bitStream->Write1();
				this->m_blockedAction->Calculate(context, bitStream, branch);
				PlayFx(u"blocked", context->caster);
				return;
			} else {
				bitStream->Write0();
			}
		}

		// DoHit
		if (tacArcInfo->targets.size() > this->m_maxTargets) tacArcInfo->targets.resize(this->m_maxTargets);
		bitStream->Write<uint32_t>(tacArcInfo->targets.size());
		if (tacArcInfo->targets.size() > 0) context->foundTarget = true;

		// write all the tacArcInfo->targets to the bitstream
		for (auto* validTarget : tacArcInfo->targets) {
			bitStream->Write(validTarget->GetObjectID());
		}

		// then case all the actions
		for (auto* validTarget : tacArcInfo->targets) {
			bitStream->Write(validTarget->GetObjectID());
			branch.target = validTarget->GetObjectID();
			this->m_action->Calculate(context, bitStream, branch);
		}
	}
}

// void DoEnvRaycast()
// void DoCheckEnv()
// void GetObjectsInsideConeAndPiesliceTacArc()
// void GetObjectsInsideFrustumTacArc()
// void SortNoWeights()
// void SortUseWeights()

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
	this->m_method = static_cast<eTacArcMethod>(GetInt("method", 1));
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
	if (this->m_maxTargets == 0) this->m_maxTargets == 100;

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
