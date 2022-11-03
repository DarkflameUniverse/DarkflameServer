#include "RaceMaelstromGeiser.h"
#include "GameMessages.h"
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
		player = EntityManager::Instance()->GetEntity(possessableComponent->GetPossessor());

		if (player == nullptr) {
			return;
		}

		vehicle = entering;
	} else if (entering->IsPlayer()) {
		auto* possessorComponent = entering->GetComponent<PossessorComponent>();

		if (possessorComponent == nullptr) {
			return;
		}

		vehicle = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (vehicle == nullptr) {
			return;
		}

		player = entering;
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

void RaceMaelstromGeiser::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "downTime") {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 4048, u"rebuild_medium", "geiser", LWOOBJID_EMPTY, 1, 1, true);

		self->AddTimer("buildUpTime", 1);
	} else if (timerName == "buildUpTime") {
		self->SetVar(u"AmFiring", true);

		self->AddTimer("killTime", 1.5f);
	} else if (timerName == "killTime") {
		GameMessages::SendStopFXEffect(self, true, "geiser");

		self->SetVar(u"AmFiring", false);

		self->AddTimer("downTime", 3.0);
	}
}
