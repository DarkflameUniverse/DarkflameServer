#include "RaceShipLapColumnsServer.h"

#include "RacingControlComponent.h"
#include "MovingPlatformComponent.h"

void RaceShipLapColumnsServer::OnStartup(Entity* self) {
	self->SetVar(u"Lap2Complete", false);
	self->SetVar(u"Lap3Complete", false);
}

void SetMovingToWaypoint(const int32_t waypointIndex, const std::string group) {
	const auto entities = Game::entityManager->GetEntitiesInGroup(group);
	if (entities.empty()) return;

	auto* entity = entities[0];
	entity->SetIsGhostingCandidate(false);

	auto* movingPlatfromComponent = entity->GetComponent<MovingPlatformComponent>();
	if (!movingPlatfromComponent) return;

	movingPlatfromComponent->SetSerialized(true);
	movingPlatfromComponent->GotoWaypoint(waypointIndex);
	Game::entityManager->SerializeEntity(entity);
}

void RaceShipLapColumnsServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target) return;

	const auto racingControllers = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::RACING_CONTROL);
	if (racingControllers.empty()) return;

	auto* racingControlComponent = racingControllers[0]->GetComponent<RacingControlComponent>();
	if (!racingControlComponent) return;

	const auto* player = racingControlComponent->GetPlayerData(target->GetObjectID());
	if (!player) return;

	if (player->lap == 1 && !self->GetVar<bool>(u"Lap2Complete")) {
		self->SetVar(u"Lap2Complete", true);
		SetMovingToWaypoint(1, "Lap2Column");
		SetMovingToWaypoint(0, "Lap2Ramp");
	} else if (player->lap == 2 && !self->GetVar<bool>(u"Lap3Complete")) {
		self->SetVar(u"Lap3Complete", true);
		SetMovingToWaypoint(1, "Lap3Column");
		SetMovingToWaypoint(0, "Lap3Ramp");
	}
}
