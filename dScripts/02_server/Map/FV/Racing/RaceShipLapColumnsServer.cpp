#include "RaceShipLapColumnsServer.h"

void RaceShipLapColumnsServer::OnStartup(Entity* self) {
	self->SetVar("Lap2Complete",false);
	self->SetVar("Lap3Complete",false);
}

void RaceShipLapColumnsServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;
	const auto* racingController = racingControllers.at(0);

	auto* racingControlComponent = racingController->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;
	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if(!player) return;
	
	if (player->lap == 2 && self->GetVar<bool>("Lap2Complete")) {
		self->SetVar("Lap2Complete",true);
		const auto Lap2Column = Game::entityManager->GetEntitiesInGroup("Lap2Column").at(0);
		const auto Lap2Ramp = Game::entityManager->GetEntitiesInGroup("Lap2Ramp").at(0);
	} else if  (player->lap == 3 && self->GetVar<bool>("Lap3Complete")) {
		self->SetVar("Lap3Complete",true);
		const auto Lap3Column = Game::entityManager->GetEntitiesInGroup("Lap3Column").at(0);
		const auto Lap3Ramp = Game::entityManager->GetEntitiesInGroup("Lap3Ramp").at(0);
	}
}