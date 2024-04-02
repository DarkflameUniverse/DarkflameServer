#include "SwitchBehavior.h"
#include "BehaviorBranchContext.h"
#include "EntityManager.h"
#include "Logger.h"
#include "DestroyableComponent.h"
#include "BehaviorContext.h"
#include "BuffComponent.h"

void SwitchBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, const BehaviorBranchContext branch) {
	bool state = true;

	if (m_imagination > 0 || m_targetHasBuff > 0 || m_Distance > -1.0f) {
		if (!bitStream.Read(state)) {
			LOG("Unable to read state from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
			return;
		};
	}

	auto* entity = Game::entityManager->GetEntity(context->originator);

	if (!entity) return;

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

	if (destroyableComponent) {
		if (m_isEnemyFaction) {
			auto* target = Game::entityManager->GetEntity(branch.target);
			if (target) state = destroyableComponent->IsEnemy(target);
		}

		LOG_DEBUG("[%i] State: (%d), imagination: (%i) / (%f)", entity->GetLOT(), state, destroyableComponent->GetImagination(), destroyableComponent->GetMaxImagination());
	}

	auto* behaviorToCall = state ? m_actionTrue : m_actionFalse;
	behaviorToCall->Handle(context, bitStream, branch);
}

void SwitchBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	bool state = true;
	if (m_imagination > 0 || m_targetHasBuff > 0 || m_Distance > -1.0f) {
		auto* entity = Game::entityManager->GetEntity(branch.target);

		state = entity != nullptr;

		if (state) {
			if (m_targetHasBuff != 0) {
				auto* buffComponent = entity->GetComponent<BuffComponent>();

				if (buffComponent != nullptr && !buffComponent->HasBuff(m_targetHasBuff)) {
					state = false;
				}
			} else if (m_imagination > 0) {
				auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

				if (destroyableComponent && destroyableComponent->GetImagination() < m_imagination) {
					state = false;
				}
			} else if (m_Distance > -1.0f) {
				auto* originator = Game::entityManager->GetEntity(context->originator);

				if (originator) {
					const auto distance = (originator->GetPosition() - entity->GetPosition()).Length();

					state = distance <= m_Distance;
				}
			}
		}

		bitStream.Write(state);
	}

	auto* behaviorToCall = state ? m_actionTrue : m_actionFalse;
	behaviorToCall->Calculate(context, bitStream, branch);
}

void SwitchBehavior::Load() {
	this->m_actionTrue = GetAction("action_true");

	this->m_actionFalse = GetAction("action_false");

	this->m_imagination = GetInt("imagination");

	this->m_isEnemyFaction = GetBoolean("isEnemyFaction");

	this->m_targetHasBuff = GetInt("target_has_buff", -1);

	this->m_Distance = GetFloat("distance", -1.0f);
}
