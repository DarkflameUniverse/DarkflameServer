#include "StinkyFishTarget.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "Entity.h"

void StinkyFishTarget::OnStartup(Entity* self) {
	auto position = self->GetPosition();
	position.SetY(position.GetY() - 0.5f);
	self->SetPosition(position);
}

void StinkyFishTarget::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "stinkfish" || self->GetVar<bool>(u"used"))
		return;

	self->SetVar<bool>(u"used", true);
	self->SetVar<LWOOBJID>(u"player", caster->GetObjectID());

	EntityInfo entityInfo{};
	entityInfo.lot = SHARK_LOT;
	entityInfo.pos = self->GetPosition();
	entityInfo.rot = self->GetRotation();
	entityInfo.spawnerID = self->GetObjectID();
	entityInfo.settings = {
		new LDFData<bool>(u"no_timed_spawn", true)
	};

	auto* fish = Game::entityManager->CreateEntity(entityInfo);
	Game::entityManager->ConstructEntity(fish);

	self->SetVar<LWOOBJID>(u"fish", fish->GetObjectID());
	self->AddTimer("smash", 5.0f);
}

void StinkyFishTarget::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "smash") {
		const auto playerID = self->GetVar<LWOOBJID>(u"player");
		auto* fish = Game::entityManager->GetEntity(self->GetVar<LWOOBJID>(u"fish"));

		if (fish) {
			fish->Smash(playerID);
			self->Smash(playerID);
		}
	}
}
