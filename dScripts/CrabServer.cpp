#include "CrabServer.h"
#include "PetComponent.h"

void CrabServer::OnStartup(Entity* self) {
	auto* petComponent = self->GetComponent<PetComponent>();
	if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
		return;

	// Triggers the local crab script for taming etc.
	auto tamer = self->GetVar<LWOOBJID>(u"tamer");
	// Client compares this with player:GetID() which is a string, so we'll have to give it a string
	self->SetNetworkVar(u"crabtamer", std::to_string(tamer));

	// Kill if the player decides that the crab is not worthy
	self->AddTimer("killself", 45.0f);
}

void CrabServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killself") {

		// Don't accidentally kill a pet that is already owned
		auto* petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
			return;

		self->Smash(self->GetObjectID(), SILENT);
	}
}

void CrabServer::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) {
	if (type == NOTIFY_TYPE_BEGIN) {
		self->CancelTimer("killself");
	} else if (type == NOTIFY_TYPE_QUIT || type == NOTIFY_TYPE_FAILED) {
		self->Smash(self->GetObjectID(), SILENT);
	} else if (type == NOTIFY_TYPE_SUCCESS) {
		auto* petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr)
			return;
		// TODO: Remove custom group?
		// Command the pet to the player as it may otherwise go to its spawn point which is non existant
		// petComponent->Command(NiPoint3::ZERO, LWOOBJID_EMPTY, 6, 202, true);
	}
}
