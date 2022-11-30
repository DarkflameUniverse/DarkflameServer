#include "EnemyRoninSpawner.h"
#include "SkillComponent.h"
#include "RenderComponent.h"
#include "EntityManager.h"

void EnemyRoninSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(15, "ronin");
}

void EnemyRoninSpawner::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "hatchTime") {
		auto* renderComponent = self->GetComponent<RenderComponent>();

		if (renderComponent != nullptr) {
			renderComponent->PlayEffect(644, u"create", "BurstFX1");
		}

		EntityInfo info{};
		info.lot = 7815;
		info.pos = self->GetPosition();
		info.rot = self->GetRotation();
		info.spawnerID = self->GetObjectID();

		auto* spawnedEntity = EntityManager::Instance()->CreateEntity(info);

		if (spawnedEntity == nullptr) {
			return;
		}

		EntityManager::Instance()->ConstructEntity(spawnedEntity);

		spawnedEntity->AddCallbackTimer(60, [spawnedEntity]() {
			spawnedEntity->Smash(spawnedEntity->GetObjectID());
			});

		self->Smash(self->GetObjectID());
	}
}

void EnemyRoninSpawner::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (entering->IsPlayer() && name == "ronin" && status == "ENTER" && !self->GetVar<bool>(u"hatching")) {
		StartHatching(self);

		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->CalculateBehavior(305, 3568, LWOOBJID_EMPTY);
		}
	}
}

void EnemyRoninSpawner::OnHit(Entity* self, Entity* attacker) {
	if (!self->GetVar<bool>(u"hatching")) {
		StartHatching(self);
	}
}

void EnemyRoninSpawner::StartHatching(Entity* self) {
	self->SetVar(u"hatching", true);

	auto* renderComponent = self->GetComponent<RenderComponent>();

	if (renderComponent != nullptr) {
		renderComponent->PlayEffect(2260, u"rebuild_medium", "WakeUpFX1");
	}

	self->AddTimer("hatchTime", 2);
}
