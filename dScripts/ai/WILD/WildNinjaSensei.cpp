#include "WildNinjaSensei.h"
#include "Entity.h"
#include "EffectsMessages.h"

void WildNinjaSensei::OnStartup(Entity* self) {
	GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
	self->AddTimer("CraneStart", 5);
}

void WildNinjaSensei::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "CraneStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Crane");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("TigerStart", 25);
		GameMessages::PlayAnimation(self->GetObjectID(), u"crane").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == "TigerStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Tiger");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("MantisStart", 25);
		GameMessages::PlayAnimation(self->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == "MantisStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		GameMessages::PlayAnimation(self->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Mantis");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("CraneStart", 25);
		GameMessages::PlayAnimation(self->GetObjectID(), u"mantis").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == "Bow") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Bow");
		GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}
}

