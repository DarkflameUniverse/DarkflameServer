#include "NtSentinelWalkwayServer.h"
#include "PhantomPhysicsComponent.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "ePhysicsEffectType.h"

void NtSentinelWalkwayServer::OnStartup(Entity* self) {
	auto* phantomPhysicsComponent = self->GetComponent<PhantomPhysicsComponent>();

	if (phantomPhysicsComponent == nullptr) {
		return;
	}

	auto force = self->GetVar<int32_t>(u"force");

	if (force == 0) {
		force = 115;
	}

	const auto forward = self->GetRotation().GetRightVector() * -1;

	phantomPhysicsComponent->SetEffectType(ePhysicsEffectType::PUSH);
	phantomPhysicsComponent->SetDirectionalMultiplier(force);
	phantomPhysicsComponent->SetDirection(forward);
	phantomPhysicsComponent->SetPhysicsEffectActive(true);

	Game::entityManager->SerializeEntity(self);

	self->SetProximityRadius(3, "speedboost");
}

void NtSentinelWalkwayServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "speedboost" || !entering->IsPlayer() || status != "ENTER") {
		return;
	}

	auto* player = entering;

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->Progress(eMissionTaskType::SCRIPT, self->GetLOT());
	}
}
