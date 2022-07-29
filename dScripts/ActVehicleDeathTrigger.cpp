#include "ActVehicleDeathTrigger.h"
#include "PossessableComponent.h"
#include "GameMessages.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "PossessorComponent.h"


void ActVehicleDeathTrigger::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* possessableComponent = target->GetComponent<PossessableComponent>();

	Entity* vehicle;
	Entity* player;

	if (possessableComponent != nullptr) {
		auto* player = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());

		if (player == nullptr) {
			return;
		}

		return;
	} else if (target->IsPlayer()) {
		auto* possessorComponent = target->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		player = target;
	} else {
		return;
	}


	GameMessages::SendDie(vehicle, self->GetObjectID(), LWOOBJID_EMPTY, true, VIOLENT, u"", 0, 0, 0, true, false, 0);

	auto* zoneController = dZoneManager::Instance()->GetZoneControlObject();

	auto* racingControlComponent = zoneController->GetComponent<RacingControlComponent>();

	if (racingControlComponent != nullptr) {
		racingControlComponent->OnRequestDie(player);
	}
}
