#include "RaceMaelstromGeiser.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "EntityManager.h"
#include "RacingControlComponent.h"
#include "dZoneManager.h"

void RaceMaelstromGeiser::OnStartup(Entity* self) {
	self->SetVar(u"AmFiring", false);

	self->AddTimer("downTime", self->GetVar<int32_t>(u"startTime"));

	self->SetProximityRadius(15, "deathZone");
}

void RaceMaelstromGeiser::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (!entering->IsPlayer() || name != "deathZone" || status != "ENTER") {
		return;
	}

	if (!self->GetVar<bool>(u"AmFiring")) {
		return;
	}

	auto* possessableComponent = entering->GetComponent<PossessableComponent>();

	Entity* vehicle;
	Entity* player;

	if (possessableComponent != nullptr) {
		player = Game::entityManager->GetEntity(possessableComponent->GetPossessor());

		if (player == nullptr) {
			return;
		}

		vehicle = entering;
	} else if (entering->IsPlayer()) {
		auto* possessorComponent = entering->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		vehicle = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		player = entering;
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

void RaceMaelstromGeiser::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "downTime") {
		GameMessages::PlayFXEffect(self->GetObjectID(), 4048, u"rebuild_medium", "geiser").Send(UNASSIGNED_SYSTEM_ADDRESS);

		self->AddTimer("buildUpTime", 1);
	} else if (timerName == "buildUpTime") {
		self->SetVar(u"AmFiring", true);

		self->AddTimer("killTime", 1.5f);
	} else if (timerName == "killTime") {
		GameMessages::StopFXEffect(self->GetObjectID(), true, "geiser").Send(UNASSIGNED_SYSTEM_ADDRESS);

		self->SetVar(u"AmFiring", false);

		self->AddTimer("downTime", 3.0);
	}
}
