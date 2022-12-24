#include "InterruptBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"

void InterruptBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	LWOOBJID usedTarget = m_target ? branch.target : context->caster;
	if (usedTarget == LWOOBJID_EMPTY) {
		return;
	}

	if (usedTarget != context->caster) {
		bool targetIsImmune = false;
		if (!bitStream->Read(targetIsImmune)) {
			Game::logger->Log("InterruptBehavior", "Unable to read unknown1 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};
		Game::logger->Log("InterruptBehavior", "is immune %i", targetIsImmune);
		if (targetIsImmune) return;
	}

	if (!this->m_InterruptBlock) {
		bool isBlocking = false;

		if (!bitStream->Read(isBlocking)) {
			Game::logger->Log("InterruptBehavior", "Unable to read unknown2 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};
		Game::logger->Log("InterruptBehavior", "target is blocking %i", isBlocking);
		if (isBlocking) return;
	}

	bool unknown = false;

	if (!bitStream->Read(unknown)) {
		Game::logger->Log("InterruptBehavior", "Unable to read unknown3 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};

	Game::logger->Log("InterruptBehavior", "unknown %i", unknown);

	while (unknown) {
		uint32_t test;
		if (!bitStream->Read(test)) {
			Game::logger->Log("InterruptBehavior", "out of bits");
		}
		Game::logger->Log("InterruptBehavior", "possible read?	%i", test);
	}

	if (branch.target == context->originator) return;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;
	skillComponent->Interrupt(m_InterruptAttack, m_InterruptBlock, m_interruptCharge);
}


void InterruptBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	LWOOBJID usedTarget = m_target ? branch.target : context->caster;

	if (usedTarget == LWOOBJID_EMPTY) {
		return;
	}

	if (usedTarget != context->originator) {
		bitStream->Write(false);
	}

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* destroyableComponent = target->GetComponent<DestroyableComponent>();
	if (!destroyableComponent) {
		Game::logger->Log("InterruptBehavior", "no destroyable?");
		return;
	}

	if (!this->m_InterruptBlock) {
		bool isBlocking = destroyableComponent->GetAttacksToBlock() > 0 || destroyableComponent->GetIsShielded();
		bitStream->Write(isBlocking);
		if (isBlocking) return;
	}

	bitStream->Write(false);

	if (branch.target == context->originator) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;
	skillComponent->Interrupt(m_InterruptAttack, m_InterruptBlock, m_interruptCharge);
}


void InterruptBehavior::Load() {
	this->m_target = GetBoolean("target");

	this->m_InterruptAttack = GetBoolean("interrupt_attack");

	this->m_InterruptBlock = GetBoolean("interrupt_block");

	this->m_interruptCharge = GetBoolean("interrupt_charge");
}
