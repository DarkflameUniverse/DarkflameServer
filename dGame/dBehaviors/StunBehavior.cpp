#include "StunBehavior.h"

#include "BaseCombatAIComponent.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "Logger.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"


void StunBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	if (this->m_stunCaster || branch.target == context->originator) {
		return;
	}

	bool blocked{};
	if (!bitStream->Read(blocked)) {
		Log("Unable to read blocked from bitStream, aborting Handle! %i", bitStream->GetNumberOfUnreadBits());
		return;
	};

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target == nullptr) {
		Log("Failed to find target (%llu)!", branch.target);

		return;
	}

	/*
	 * If our target is an enemy we can go ahead and stun it.
	 */

	auto* combatAiComponent = static_cast<BaseCombatAIComponent*>(target->GetComponent(eReplicaComponentType::BASE_COMBAT_AI));

	if (combatAiComponent == nullptr) {
		return;
	}

	combatAiComponent->Stun(branch.duration);
}

void StunBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	if (this->m_stunCaster || branch.target == context->originator) {
		auto* self = EntityManager::Instance()->GetEntity(context->originator);

		if (self == nullptr) {
			Log("Invalid self entity (%llu)!", context->originator);

			return;
		}

		/*
		 * See if we can stun ourselves
		 */

		auto* combatAiComponent = static_cast<BaseCombatAIComponent*>(self->GetComponent(eReplicaComponentType::BASE_COMBAT_AI));

		if (combatAiComponent == nullptr) {
			return;
		}

		combatAiComponent->Stun(branch.duration);

		return;
	}

	bool blocked = false;

	auto* target = EntityManager::Instance()->GetEntity(branch.target);

	if (target != nullptr) {
		auto* destroyableComponent = target->GetComponent<DestroyableComponent>();

		if (destroyableComponent != nullptr) {
			blocked = destroyableComponent->IsKnockbackImmune();
		}
	}

	bitStream->Write(blocked);

	if (target == nullptr) {
		Log("Failed to find target (%llu)!", branch.target);

		return;
	}

	/*
	 * If our target is an enemy we can go ahead and stun it.
	 */

	auto* combatAiComponent = static_cast<BaseCombatAIComponent*>(target->GetComponent(eReplicaComponentType::BASE_COMBAT_AI));

	if (combatAiComponent == nullptr) {
		return;
	}

	combatAiComponent->Stun(branch.duration);
}

void StunBehavior::Load() {
	this->m_stunCaster = GetBoolean("stun_caster");
}
