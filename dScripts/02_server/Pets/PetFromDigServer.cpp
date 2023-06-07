#include "PetFromDigServer.h"
#include "PetComponent.h"
#include "ePetTamingNotifyType.h"

void PetFromDigServer::OnStartup(Entity* self) {
	auto petComponent = self->GetComponent<PetComponent>();
	if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
		return;

	// Triggers the local dig pet script for taming etc.
	auto tamer = self->GetVar<LWOOBJID>(u"tamer");

	// Client compares this with player:GetID() which is a string, so we'll have to give it a string
	self->SetNetworkVar(u"pettamer", std::to_string(tamer));

	// Kill if the player decides that the dig pet is not worthy
	self->AddTimer("killself", 45.0f);
}

void PetFromDigServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killself") {

		// Don't accidentally kill a pet that is already owned
		auto petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
			return;

		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}

void PetFromDigServer::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, ePetTamingNotifyType type) {
	if (type == ePetTamingNotifyType::BEGIN) {
		self->CancelTimer("killself");
	} else if (type == ePetTamingNotifyType::QUIT || type == ePetTamingNotifyType::FAILED) {
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	} else if (type == ePetTamingNotifyType::SUCCESS) {
		auto petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr)
			return;
		// TODO: Remove custom group?
		// Command the pet to the player as it may otherwise go to its spawn point which is non existant
		// petComponent->Command(NiPoint3::ZERO, LWOOBJID_EMPTY, 6, 202, true);
	}
}
