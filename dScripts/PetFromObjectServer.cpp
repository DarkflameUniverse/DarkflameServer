#include "PetFromObjectServer.h"
#include "PetComponent.h"

void PetFromObjectServer::OnStartup(Entity* self) {
	self->SetNetworkVar(u"pettamer", std::to_string(self->GetVar<LWOOBJID>(u"tamer")));
	self->AddTimer("killSelf", 45.0f);
}

void PetFromObjectServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killSelf") {
		const auto* petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
			return;
		self->Smash(self->GetObjectID(), SILENT);
	}
}

void PetFromObjectServer::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) {
	switch (type) {
	case NOTIFY_TYPE_BEGIN:
		self->CancelAllTimers();
		break;
	case NOTIFY_TYPE_QUIT:
	case NOTIFY_TYPE_FAILED:
		self->Smash(self->GetObjectID(), SILENT);
		break;
	case NOTIFY_TYPE_SUCCESS:
		// TODO: Remove from groups?
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"UpdateSuccessPicking", 0,
			0, tamer->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
	default:
		break;
	}
}
