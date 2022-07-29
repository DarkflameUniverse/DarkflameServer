#include "EnemySkeletonSpawner.h"
#include "SkillComponent.h"
#include "RenderComponent.h"
#include "EntityManager.h"

void EnemySkeletonSpawner::OnStartup(Entity* self) {
	self->SetProximityRadius(15, "ronin");

	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent != nullptr) {
		skillComponent->CalculateBehavior(1127, 24812, LWOOBJID_EMPTY, true);
	}
}

void EnemySkeletonSpawner::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "hatchTime") {
		auto* renderComponent = self->GetComponent<RenderComponent>();

		if (renderComponent != nullptr) {
			renderComponent->PlayEffect(644, u"create", "BurstFX1");
		}

		EntityInfo info{};
		info.lot = 14024;
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

void EnemySkeletonSpawner::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (entering->IsPlayer() && name == "ronin" && status == "ENTER" && !self->GetVar<bool>(u"hatching")) {
		StartHatching(self);

		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (skillComponent != nullptr) {
			skillComponent->CalculateBehavior(305, 3568, LWOOBJID_EMPTY);
		}
	}
}

void EnemySkeletonSpawner::OnHit(Entity* self, Entity* attacker) {
	if (!self->GetVar<bool>(u"hatching")) {
		StartHatching(self);
	}
}

void EnemySkeletonSpawner::StartHatching(Entity* self) {
	self->SetVar(u"hatching", true);

	auto* renderComponent = self->GetComponent<RenderComponent>();

	if (renderComponent != nullptr) {
		renderComponent->PlayEffect(9017, u"cast", "WakeUpFX1");
		renderComponent->PlayEffect(9018, u"burst", "WakeUpFX1");
	}

	self->AddTimer("hatchTime", 2);
}
