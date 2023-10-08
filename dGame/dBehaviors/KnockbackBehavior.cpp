#define _USE_MATH_DEFINES
#include <cmath>
#include "KnockbackBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "DestroyableComponent.h"
#include "Game.h"
#include "dLogger.h"

void KnockbackBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	bool unknown{};

	if (!bitStream->Read(unknown)) {
		Game::logger->Log("KnockbackBehavior", "Unable to read unknown from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};
}

void KnockbackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	bool blocked = false;

	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target != nullptr) {
		auto* destroyableComponent = target->GetComponent<DestroyableComponent>();

		if (destroyableComponent != nullptr) {
			blocked = destroyableComponent->IsKnockbackImmune();
		}
	}

	bitStream->Write(blocked);
}

void KnockbackBehavior::Load() {
	this->m_strength = GetInt("strength");
	this->m_angle = GetInt("angle");
	this->m_relative = GetBoolean("relative");
	this->m_time = GetInt("time_ms");
}
