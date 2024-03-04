#include "OverTimeBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "Game.h"
#include "Logger.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "CDClientDatabase.h"
#include "CDClientManager.h"

#include "CDSkillBehaviorTable.h"

void OverTimeBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	const auto originator = context->originator;

	auto* entity = Game::entityManager->GetEntity(originator);

	if (entity == nullptr) return;

	for (size_t i = 0; i < m_NumIntervals; i++) {
		entity->AddCallbackTimer((i + 1) * m_Delay, [originator, branch, this]() {
			auto* entity = Game::entityManager->GetEntity(originator);

			if (entity == nullptr) return;

			auto* skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent == nullptr) return;

			skillComponent->CalculateBehavior(m_Action, m_ActionBehaviorId, branch.target, true, true);
			});
	}
}

void OverTimeBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {

}

void OverTimeBehavior::Load() {
	m_Action = GetInt("action");
	// Since m_Action is a skillID and not a behavior, get is correlated behaviorID.

	CDSkillBehaviorTable* skillTable = CDClientManager::GetTable<CDSkillBehaviorTable>();
	m_ActionBehaviorId = skillTable->GetSkillByID(m_Action).behaviorID;

	m_Delay = GetFloat("delay");
	m_NumIntervals = GetInt("num_intervals");
}
