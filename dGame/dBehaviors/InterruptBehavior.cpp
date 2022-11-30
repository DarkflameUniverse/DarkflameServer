#include "InterruptBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "dLogger.h"
#include "EntityManager.h"
#include "SkillComponent.h"


void InterruptBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (branch.target != context->originator) {
		bool unknown = false;

		bitStream->Read(unknown);

		if (unknown) return;
	}

	if (!this->m_interruptBlock) {
		bool unknown = false;

		bitStream->Read(unknown);

		if (unknown) return;
	}

	if (this->m_target) // Guess...
	{
		bool unknown = false;

		bitStream->Read(unknown);
	}

	if (branch.target == context->originator) return;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;

	skillComponent->Interrupt();
}


void InterruptBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	if (branch.target != context->originator) {
		bitStream->Write(false);
	}

	if (!this->m_interruptBlock) {
		bitStream->Write(false);
	}

	bitStream->Write(false);

	if (branch.target == context->originator) return;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;

	skillComponent->Interrupt();
}


void InterruptBehavior::Load() {
	this->m_target = GetBoolean("target");

	this->m_interruptBlock = GetBoolean("interrupt_block");
}
