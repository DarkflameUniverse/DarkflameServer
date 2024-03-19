#include "DarkInspirationBehavior.h"

#include "BehaviorBranchContext.h"
#include "Entity.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "BehaviorContext.h"

void DarkInspirationBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, const BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		LOG_DEBUG("Failed to find target (%llu)!", branch.target);
		return;
	}

	auto* destroyableComponent = target->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr) {
		return;
	}

	if (destroyableComponent->HasFaction(m_FactionList)) {
		this->m_ActionIfFactionMatches->Handle(context, bitStream, branch);
	}
}

void DarkInspirationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) {
		LOG_DEBUG("Failed to find target (%llu)!", branch.target);

		return;
	}

	auto* destroyableComponent = target->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr) {
		return;
	}

	if (destroyableComponent->HasFaction(m_FactionList)) {
		this->m_ActionIfFactionMatches->Calculate(context, bitStream, branch);
	}
}

void DarkInspirationBehavior::Load() {
	this->m_ActionIfFactionMatches = GetAction("action");

	this->m_FactionList = GetInt("faction_list");
}
