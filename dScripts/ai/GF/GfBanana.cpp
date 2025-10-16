#include "GfBanana.h"

#include "Entity.h"
#include "DestroyableComponent.h"
#include "EntityInfo.h"
#include "EntityManager.h"

void GfBanana::SpawnBanana(Entity* self) {
	auto position = self->GetPosition();
	const auto rotation = self->GetRotation();

	position.y += 12;
	position.x -= QuatUtils::Right(rotation).x * 5;
	position.z -= QuatUtils::Right(rotation).z * 5;

	EntityInfo info{};

	info.pos = position;
	info.rot = rotation;
	info.lot = 6909;
	info.spawnerID = self->GetObjectID();

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

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

	auto* bananaEntity = Game::entityManager->GetEntity(bananaId);

	if (bananaEntity == nullptr) {
		self->SetVar(u"banana", LWOOBJID_EMPTY);

		self->AddTimer("bananaTimer", 30);

		return;
	}
	bananaEntity->Smash(LWOOBJID_EMPTY, eKillType::SILENT);

	const auto rotation = self->GetRotation();
	EntityInfo info{};
	info.lot = 6718;
	info.pos = self->GetPosition();
	info.pos.y += 12;
	info.pos.x -= QuatUtils::Right(rotation).x * 5;
	info.pos.z -= QuatUtils::Right(rotation).z * 5;
	info.rot = rotation;
	info.spawnerID = self->GetObjectID();
	info.settings = { new LDFData<uint32_t>(u"motionType", 5) };
	auto* const newEn = Game::entityManager->CreateEntity(info, nullptr, self);
	Game::entityManager->ConstructEntity(newEn);
}

void GfBanana::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "bananaTimer") {
		SpawnBanana(self);
	}
}
