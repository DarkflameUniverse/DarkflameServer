#include "WildNinjaSensei.h"
#include "Entity.h"

void WildNinjaSensei::OnStartup(Entity* self) {
	GameMessages::SendPlayAnimation(self, u"bow");
	self->AddTimer("CraneStart", 5);
}

void WildNinjaSensei::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "CraneStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Crane");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("TigerStart", 25);
		GameMessages::SendPlayAnimation(self, u"crane");
	} else if (timerName == "TigerStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		GameMessages::SendPlayAnimation(self, u"bow");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Tiger");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("MantisStart", 25);
		GameMessages::SendPlayAnimation(self, u"tiger");
	} else if (timerName == "MantisStart") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		GameMessages::SendPlayAnimation(self, u"tiger");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Mantis");
		self->AddTimer("Bow", 15.5f);
		self->AddTimer("CraneStart", 25);
		GameMessages::SendPlayAnimation(self, u"mantis");
	} else if (timerName == "Bow") {
		auto ninjas = Game::entityManager->GetEntitiesInGroup("Ninjastuff");
		for (auto ninja : ninjas) ninja->NotifyObject(self, "Bow");
		GameMessages::SendPlayAnimation(self, u"bow");
	}
}

