#include "GfBanana.h"

#include "Entity.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"

void GfBanana::SpawnBanana(Entity* self) {
	auto position = self->GetPosition();
	const auto rotation = self->GetRotation();

	position.y += 12;
	position.x -= rotation.GetRightVector().x * 5;
	position.z -= rotation.GetRightVector().z * 5;

	EntityInfo info{};

	info.pos = position;
	info.rot = rotation;
	info.lot = 6909;
	info.spawnerID = self->GetObjectID();

	auto* entity = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(entity);

	self->SetVar(u"banana", entity->GetObjectID());

	entity->AddDieCallback([self]() {
		self->SetVar(u"banana", LWOOBJID_EMPTY);

		self->AddTimer("bananaTimer", 30);
		});
}

void GfBanana::OnStartup(Entity* self) {
	SpawnBanana(self);
}

void GfBanana::OnHit(Entity* self, Entity* attacker) {
	auto* destroyable = self->GetComponent<DestroyableComponent>();

	destroyable->SetHealth(9999);

	const auto bananaId = self->GetVar<LWOOBJID>(u"banana");

	if (bananaId == LWOOBJID_EMPTY) return;

	auto* bananaEntity = EntityManager::Instance()->GetEntity(bananaId);

	if (bananaEntity == nullptr) {
		self->SetVar(u"banana", LWOOBJID_EMPTY);

		self->AddTimer("bananaTimer", 30);

		return;
	}

	bananaEntity->SetPosition(bananaEntity->GetPosition() - NiPoint3::UNIT_Y * 8);

	auto* bananaDestroyable = bananaEntity->GetComponent<DestroyableComponent>();

	bananaDestroyable->SetHealth(0);

	bananaDestroyable->Smash(attacker->GetObjectID());

	/*
	auto position = self->GetPosition();
	const auto rotation = self->GetRotation();

	position.y += 12;
	position.x -= rotation.GetRightVector().x * 5;
	position.z -= rotation.GetRightVector().z * 5;

	EntityInfo info {};

	info.pos = position;
	info.rot = rotation;
	info.lot = 6718;
	info.spawnerID = self->GetObjectID();

	auto* entity = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(entity, UNASSIGNED_SYSTEM_ADDRESS);
	*/

	EntityManager::Instance()->SerializeEntity(self);
}

void GfBanana::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "bananaTimer") {
		SpawnBanana(self);
	}
}
