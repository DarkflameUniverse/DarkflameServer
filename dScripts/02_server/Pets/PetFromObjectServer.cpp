#include "PetFromObjectServer.h"
#include "PetComponent.h"
#include "ePetTamingNotifyType.h"

void PetFromObjectServer::OnStartup(Entity* self) {
	self->SetNetworkVar(u"pettamer", std::to_string(self->GetVar<LWOOBJID>(u"tamer")));
	self->AddTimer("killSelf", 45.0f);
}

void PetFromObjectServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killSelf") {
		const auto petComponent = self->GetComponent<PetComponent>();
		if (petComponent == nullptr || petComponent->GetOwner() != nullptr)
			return;
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}

void PetFromObjectServer::OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, ePetTamingNotifyType type) {
	switch (type) {
	case ePetTamingNotifyType::BEGIN:
		self->CancelAllTimers();
		break;
	case ePetTamingNotifyType::QUIT:
	case ePetTamingNotifyType::FAILED:
		self->Smash(self->GetObjectID(), eKillType::SILENT);
		break;
	case ePetTamingNotifyType::SUCCESS:
		// TODO: Remove from groups?
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"UpdateSuccessPicking", 0,
			0, tamer->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
	default:
		break;
	}
}
