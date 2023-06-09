#include "CarBoostBehavior.h"
#include "BehaviorBranchContext.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "BehaviorContext.h"
#include "CharacterComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "PossessableComponent.h"

void CarBoostBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	GameMessages::SendVehicleAddPassiveBoostAction(branch.target, UNASSIGNED_SYSTEM_ADDRESS);

	auto* entity = EntityManager::Instance()->GetEntity(context->originator);

	if (entity == nullptr) {
		return;
	}

	Game::logger->Log("Car boost", "Activating car boost!");

	auto* possessableComponent = entity->GetComponent<PossessableComponent>();
	if (possessableComponent != nullptr) {

		auto* possessor = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());
		if (possessor != nullptr) {

			auto* characterComponent = possessor->GetComponent<CharacterComponent>();
			if (characterComponent != nullptr) {
				Game::logger->Log("Car boost", "Tracking car boost!");
				characterComponent->UpdatePlayerStatistic(RacingCarBoostsActivated);
			}
		}
	}


	m_Action->Handle(context, bitStream, branch);

	entity->AddCallbackTimer(m_Time, [entity]() {
		GameMessages::SendVehicleRemovePassiveBoostAction(entity->GetObjectID(), UNASSIGNED_SYSTEM_ADDRESS);
		});
}

void CarBoostBehavior::Load() {
	m_Action = GetAction("action");

	m_Time = GetFloat("time");
}
