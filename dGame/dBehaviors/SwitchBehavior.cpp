#include "SwitchBehavior.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "dLogger.h"
#include "DestroyableComponent.h"
#include "BehaviorContext.h"
#include "BuffComponent.h"

void SwitchBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, const BehaviorBranchContext branch) {
	auto state = true;

	if (this->m_imagination > 0 || !this->m_isEnemyFaction) {
		bitStream->Read(state);
	}

	auto* entity = EntityManager::Instance()->GetEntity(context->originator);

	if (entity == nullptr) {
		return;
	}

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr) {
		return;
	}

	Game::logger->Log("SwitchBehavior", "[%i] State: (%d), imagination: (%i) / (%f)", entity->GetLOT(), state, destroyableComponent->GetImagination(), destroyableComponent->GetMaxImagination());

	if (state || (entity->GetLOT() == 8092 && destroyableComponent->GetImagination() >= m_imagination)) {
		this->m_actionTrue->Handle(context, bitStream, branch);
	} else {
		this->m_actionFalse->Handle(context, bitStream, branch);
	}
}

void SwitchBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	auto state = true;

	if (this->m_imagination > 0 || !this->m_isEnemyFaction) {
		auto* entity = EntityManager::Instance()->GetEntity(branch.target);

		state = entity != nullptr;

		if (state && m_targetHasBuff != 0) {
			auto* buffComponent = entity->GetComponent<BuffComponent>();

			if (buffComponent != nullptr && !buffComponent->HasBuff(m_targetHasBuff)) {
				state = false;
			}
		}

		bitStream->Write(state);
	}

	if (state) {
		this->m_actionTrue->Calculate(context, bitStream, branch);
	} else {
		this->m_actionFalse->Calculate(context, bitStream, branch);
	}
}

void SwitchBehavior::Load() {
	this->m_actionTrue = GetAction("action_true");

	this->m_actionFalse = GetAction("action_false");

	this->m_imagination = GetInt("imagination");

	this->m_isEnemyFaction = GetBoolean("isEnemyFaction");

	this->m_targetHasBuff = GetInt("target_has_buff");
}
