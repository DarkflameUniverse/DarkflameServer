#include "ImmunityBehavior.h"

#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "DestroyableComponent.h"
#include "ControllablePhysicsComponent.h"
#include "eStateChangeType.h"

void ImmunityBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (!target) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", branch.target);
		return;
	}

	auto* destroyableComponent = target->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetStatusImmunity(
			eStateChangeType::PUSH,
			this->m_ImmuneToBasicAttack,
			this->m_ImmuneToDamageOverTime,
			this->m_ImmuneToKnockback,
			this->m_ImmuneToInterrupt,
			this->m_ImmuneToSpeed,
			this->m_ImmuneToImaginationGain,
			this->m_ImmuneToImaginationLoss,
			this->m_ImmuneToQuickbuildInterrupt,
			this->m_ImmuneToPullToPoint
		);
	}

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::PUSH,
			context->caster,
			this->m_ImmuneToStunAttack,
			this->m_ImmuneToStunEquip,
			this->m_ImmuneToStunInteract,
			this->m_ImmuneToStunJump,
			this->m_ImmuneToStunMove,
			this->m_ImmuneToStunTurn,
			this->m_ImmuneToStunUseItem
		);
	}

	context->RegisterTimerBehavior(this, branch, target->GetObjectID());
}

void ImmunityBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Handle(context, bitStream, branch);
}

void ImmunityBehavior::Timer(BehaviorContext* context, BehaviorBranchContext branch, const LWOOBJID second) {
	auto* target = EntityManager::Instance()->GetEntity(second);

	if (!target) {
		Game::logger->Log("DamageAbsorptionBehavior", "Failed to find target (%llu)!", second);
		return;
	}

	auto* destroyableComponent = target->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetStatusImmunity(
			eStateChangeType::POP,
			this->m_ImmuneToBasicAttack,
			this->m_ImmuneToDamageOverTime,
			this->m_ImmuneToKnockback,
			this->m_ImmuneToInterrupt,
			this->m_ImmuneToSpeed,
			this->m_ImmuneToImaginationGain,
			this->m_ImmuneToImaginationLoss,
			this->m_ImmuneToQuickbuildInterrupt,
			this->m_ImmuneToPullToPoint
		);
	}

	auto* controllablePhysicsComponent = target->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::POP,
			context->caster,
			this->m_ImmuneToStunAttack,
			this->m_ImmuneToStunEquip,
			this->m_ImmuneToStunInteract,
			this->m_ImmuneToStunJump,
			this->m_ImmuneToStunMove,
			this->m_ImmuneToStunTurn,
			this->m_ImmuneToStunUseItem
		);
	}

}

void ImmunityBehavior::Load() {
	//Stun
	this->m_ImmuneToStunAttack = GetBoolean("immune_stun_attack", false);
	this->m_ImmuneToStunEquip = GetBoolean("immune_stun_equip", false);
	this->m_ImmuneToStunInteract = GetBoolean("immune_stun_interact", false);
	this->m_ImmuneToStunMove = GetBoolean("immune_stun_move", false);
	this->m_ImmuneToStunTurn = GetBoolean("immune_stun_rotate", false);

	// Status
	this->m_ImmuneToBasicAttack = GetBoolean("immune_basic_attack", false);
	this->m_ImmuneToDamageOverTime = GetBoolean("immune_damage_over_time", false);
	this->m_ImmuneToKnockback = GetBoolean("immune_knockback", false);
	this->m_ImmuneToInterrupt = GetBoolean("immune_interrupt", false);
	this->m_ImmuneToSpeed = GetBoolean("immune_speed", false);
	this->m_ImmuneToImaginationGain = GetBoolean("immune_imagination_gain", false);
	this->m_ImmuneToImaginationLoss = GetBoolean("immune_imagination_loss", false);
	this->m_ImmuneToQuickbuildInterrupt = GetBoolean("immune_quickbuild_interrupts", false);
	this->m_ImmuneToPullToPoint = GetBoolean("immune_pulltopoint", false);
}
