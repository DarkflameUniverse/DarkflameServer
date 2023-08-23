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

		if (!bitStream->Read(unknown)) {
			Game::logger->Log("InterruptBehavior", "Unable to read unknown1 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};

		if (unknown) return;
	}

	if (!this->m_interruptBlock) {
		bool unknown = false;

		if (!bitStream->Read(unknown)) {
			Game::logger->Log("InterruptBehavior", "Unable to read unknown2 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};

		if (unknown) return;
	}

	if (this->m_target) // Guess...
	{
		bool unknown = false;

		if (!bitStream->Read(unknown)) {
			Game::logger->Log("InterruptBehavior", "Unable to read unknown3 from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
			return;
		};
	}

	if (branch.target == context->originator) return;

	auto* target = Game::entityManager->GetEntity(branch.target);

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

	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;

	skillComponent->Interrupt();
}


void InterruptBehavior::Load() {
	this->m_target = GetBoolean("target");

	this->m_interruptBlock = GetBoolean("interrupt_block");
}
