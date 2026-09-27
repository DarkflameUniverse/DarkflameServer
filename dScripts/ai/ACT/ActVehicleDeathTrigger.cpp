#include "ActVehicleDeathTrigger.h"
#include "PossessableComponent.h"
#include "GameMessages.h"
#include "CombatMessages.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "PossessorComponent.h"


void ActVehicleDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* possessableComponent = target->GetComponent<PossessableComponent>();

	Entity* vehicle;
	Entity* player;

	if (possessableComponent != nullptr) {
		auto* player = Game::entityManager->GetEntity(possessableComponent->GetPossessor());

		if (player == nullptr) {
			return;
		}

		return;
	} else if (target->IsPlayer()) {
		auto* possessorComponent = target->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		player = target;
	} else {
		return;
	}


	GameMessages::Die die;
	die.target = vehicle->GetObjectID();
	die.bClientDeath = true;
	die.bSpawnLoot = false;
	die.killerID = self->GetObjectID();
	die.Send(UNASSIGNED_SYSTEM_ADDRESS);

	auto* zoneController = Game::zoneManager->GetZoneControlObject();

	auto* racingControlComponent = zoneController->GetComponent<RacingControlComponent>();

	if (racingControlComponent != nullptr) {
		racingControlComponent->OnRequestDie(player);
	}
}
